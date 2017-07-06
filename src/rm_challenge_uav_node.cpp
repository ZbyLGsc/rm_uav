// ros
#include <cv_bridge/cv_bridge.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/image_encodings.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Int16.h>
#include <std_msgs/UInt8.h>
#include "std_msgs/String.h"
#include <actionlib/client/simple_action_client.h>
#include <actionlib/client/terminal_state.h>

// my file
#include "rm_challenge_fsm.h"

#if CURRENT_COMPUTER == MANIFOLD
dji_sdk::RCChannels g_rc_channels;
void rc_channels_callback(const dji_sdk::RCChannels rc_channels);
#endif

RMChallengeFSM g_fsm;
bool is_F_mode= false;

void uav_state_callback(const std_msgs::UInt8::ConstPtr &msg);
void guidance_distance_callback(const sensor_msgs::LaserScan &g_oa);
void guidance_position_callback(
    const geometry_msgs::Vector3Stamped &g_pos);
// void ultrasonic_callback(const sensor_msgs::LaserScan& g_ul) ;
void vision_pillar_callback(const std_msgs::String::ConstPtr &msg);
void vision_base_callback(const std_msgs::String::ConstPtr &msg);
void vision_line_callback(const std_msgs::String::ConstPtr &msg);
/**timer callback, control uav in a finite state machine*/
void timer_callback(const ros::TimerEvent &evt);

int main(int argc, char **argv)
{
  ros::init(argc, argv, "rm_uav_challenge");
  ros::NodeHandle node;
/*subscriber from dji node*/
#if CURRENT_COMPUTER == MANIFOLD
  ros::Subscriber rc_channels_sub=
      node.subscribe("dji_sdk/rc_channels", 1, rc_channels_callback);
#endif
  ros::Subscriber uav_state_sub=
      node.subscribe("/dji_sdk/flight_status", 1, uav_state_callback);
  ros::Subscriber guidance_distance_sub= node.subscribe(
      "/guidance/obstacle_distance", 1, guidance_distance_callback);
  ros::Subscriber guidance_position_sub= node.subscribe(
      "/guidance/position", 1, guidance_position_callback);
  // ultrasonic_sub =
  //     my_node.subscribe("/guidance/ultrasonic", 1,
  //     ultrasonic_callback);
  /*subscriber for vision*/
  ros::Subscriber vision_pillar_sub=
      node.subscribe("tpp/pillar", 1, vision_pillar_callback);
  ros::Subscriber vision_base_sub=
      node.subscribe("tpp/base", 1, vision_base_callback);
  ros::Subscriber vision_line_sub=
      node.subscribe("tpp/yellow_line", 1, vision_line_callback);
  ros::Timer timer=
      node.createTimer(ros::Duration(1.0 / 50.0), timer_callback);

  /*initialize fsm*/
  g_fsm.initialize(node);
  ROS_INFO_STREAM("initialize finish, start to run");

  /*test setter function of FSM*/
  g_fsm.setDroneState(3);
  //    g_fsm.setDroneState( 3 );
  //    g_fsm.setDroneState( 4 );
  g_fsm.setHeightFromGuidance(0);
  g_fsm.setPositionFromGuidance(7, 0);
  float x= 1, y= 1;
  // g_fsm.transformCoordinate(-90.0 / 180 * 3.1415926, x, y);
  g_fsm.setPositionFromGuidance(1, 2);
  // ros::Duration(2).sleep();
  // float pos_err[2] = { 0.07, 0.3 };
  // g_fsm.setCircleVariables( true, pos_err, 1.1 );
  // g_fsm.setCircleVariables( false, pos_err, 1.1 );
  // int tri[4] = { 1, 1, 0, 0 };
  // g_fsm.setTriangleVariables( tri );
  //    g_fsm.setBaseVariables( true, pos_err );
  //    g_fsm.setBaseVariables( false, pos_err );
  // float dis[2] = { 0.3, 0.3 };
  // float nor[2] = { 1.3, -0.4 };
  // g_fsm.setLineVariables( dis, nor );
  //    ros::Duration( 2.0 ).sleep();
  /*test*/
  ros::spin();
  return 0;
}

#if CURRENT_COMPUTER == MANIFOLD
void rc_channels_callback(const dji_sdk::RCChannels rc_channels)
{
  g_rc_channels= rc_channels;
  if(fabs(rc_channels.mode - 8000) < 0.000001)
  {
    /*F mode is 8000,P :-8000 ,A: 0*/
    if(fabs(rc_channels.gear + 10000) < 0.000001)
    {
      /* gear up is -10000 ,gear down is -4545*/
      is_F_mode= true;
    }
  }
  else
  {
    is_F_mode= false;
  }
}
#endif

void uav_state_callback(const std_msgs::UInt8::ConstPtr &msg)
{
  int flight_status= msg->data;
  g_fsm.setDroneState(flight_status);
}

void guidance_distance_callback(const sensor_msgs::LaserScan &g_oa)
{
  ROS_INFO("frame_id: %s stamp: %d\n", g_oa.header.frame_id.c_str(),
           g_oa.header.stamp.sec);
  ROS_INFO("obstacle distance: [%f %f %f %f %f]\n", g_oa.ranges[0],
           g_oa.ranges[1], g_oa.ranges[2], g_oa.ranges[3],
           g_oa.ranges[4]);
  g_fsm.setHeightFromGuidance(g_oa.ranges[0]);
}

void guidance_position_callback(
    const geometry_msgs::Vector3Stamped &g_pos)
{
  ROS_INFO("frame_id: %s stamp: %d\n", g_pos.header.frame_id.c_str(),
           g_pos.header.stamp.sec);
  g_fsm.setPositionFromGuidance(g_pos.vector.x, g_pos.vector.y);
}

// void ultrasonic_callback(const sensor_msgs::LaserScan& g_ul) {
//   ROS_INFO("frame_id: %s stamp: %d\n",
//   g_ul.header.frame_id.c_str(),
//          g_ul.header.stamp.sec);
//   for (int i = 0; i < 5; i++)
//     ROS_INFO("ultrasonic distance: [%f]  reliability: [%d]\n",
//     g_ul.ranges[i],
//            (int)g_ul.intensities[i]);
// }

void vision_pillar_callback(const std_msgs::String::ConstPtr &msg)
{
  float h;
  float pos[2];
  int tri[4];
  bool circle_found;
  std::stringstream ss(msg->data.c_str());
  ss >> tri[0] >> tri[1] >> tri[2] >> tri[3] >> circle_found >>
      pos[1] >> pos[0] >> h;
  g_fsm.setCircleVariables(circle_found, pos, h);
  g_fsm.setTriangleVariables(tri);
}

void vision_base_callback(const std_msgs::String::ConstPtr &msg)
{
  float pos[2];
  bool base_found;
  std::stringstream ss(msg->data.c_str());
  ss >> pos[1] >> pos[0];
  if(fabs(pos[0]) < 0.000000001 && fabs(pos[1]) < 0.000000001)
    base_found= false;
  else
  {
    base_found= true;
    if(fabs(pos[1]) > 0.000000001)
      pos[1]= pos[1] / 1000;
    if(fabs(pos[0]) > 0.000000001)
      pos[0]= (pos[0] / 1000) - 0.15;
  }
  g_fsm.setBaseVariables(base_found, pos);
}

void vision_line_callback(const std_msgs::String::ConstPtr &msg)
{
  std::stringstream ss(msg->data.c_str());
  float line_normal[2];
  float dist_to_line[2];
  ss >> dist_to_line[0] >> dist_to_line[1] >> line_normal[0] >>
      line_normal[1];
  g_fsm.setLineVariables(dist_to_line, line_normal);
}

void timer_callback(const ros::TimerEvent &evt)
{
  if(is_F_mode)
  {
    g_fsm.run();
  }
  else
  {
    /*not F mode, need to reset fsm*/
    g_fsm.resetAllState();
    ROS_INFO("Wait for F mode");
  }
}
