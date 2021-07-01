#include "limon_base/limon_messenger.h"
#include <limon_msgs/LimonSetting.h>
#include <limon_msgs/LimonStatus.h>
#include "limon_base/limon_params.h"
#include <math.h>

using namespace agx;
using namespace limon_msgs;

LimonROSMessenger::LimonROSMessenger(ros::NodeHandle *nh)
    : limon_(nullptr), nh_(nh) {}
LimonROSMessenger::LimonROSMessenger(LimonBase *limon, ros::NodeHandle *nh)
    : limon_(limon), nh_(nh) {}
void LimonROSMessenger::SetupSubscription() {
  odom_publisher_ =
      nh_->advertise<nav_msgs::Odometry>(odom_topic_name_, 50, true);
  status_publisher_ =
      nh_->advertise<limon_msgs::LimonStatus>("/limon_status", 10, true);

  motion_cmd_sub_ = nh_->subscribe<geometry_msgs::Twist>(
      "/cmd_vel", 5, &LimonROSMessenger::TwistCmdCallback, this);
  limon_setting_sub_ = nh_->subscribe<limon_msgs::LimonSetting>(
      "/limon_setting", 1, &LimonROSMessenger::LimonSettingCbk, this);
}

void LimonROSMessenger::TwistCmdCallback(
    const geometry_msgs::Twist::ConstPtr &msg) {
  ROS_INFO("get cmd %lf %lf", msg->linear.x, msg->angular.z);
  limon_->SetMotionCommand(msg->linear.x, msg->angular.z);
}
void LimonROSMessenger::LimonSettingCbk(
    const limon_msgs::LimonSetting::ConstPtr &msg) {
  // set motion mode
  ROS_INFO("got setting %d", msg->motion_mode);
}
void LimonROSMessenger::PublishStateToROS() {
  current_time_ = ros::Time::now();
  double dt = (current_time_ - last_time_).toSec();
  static bool init_run = true;

  if (init_run) {
    last_time_ = current_time_;
    init_run = false;
    return;
  }

  auto state = limon_->GetLimonState();

  limon_msgs::LimonStatus status_msg;

  // system state
  status_msg.header.stamp = current_time_;
  status_msg.vehicle_state = state.system_state.vehicle_state;
  status_msg.control_mode = state.system_state.control_mode;
  status_msg.error_code = state.system_state.error_code;
  status_msg.battery_voltage = state.system_state.battery_voltage;
  status_msg.current_motion_mode = state.system_state.motion_mode;
  motion_mode_ = status_msg.current_motion_mode;

  // calculate the motion state
  // linear velocity, angular velocity, central steering angle
  double l_v = 0.0, a_v = 0.0, phi = 0.0;
  // x 、y direction linear velocity , motion radius
  double x_v = 0.0, y_v = 0.0, radius = 0.0;
  double phi_i = state.motion_state.steering_angle / 180.0 * M_PI;

  switch (motion_mode_) {
    case LimonSetting::MOTION_MODE_FOUR_WHEEL_DIFF: {
    } break;
    case LimonSetting::MOTION_MODE_ACKERMANN: {
    } break;
    default:
      ROS_INFO("motion mode not support: %d", motion_mode_);
      break;
  }

  status_msg.linear_velocity = l_v;
  status_msg.angular_velocity = 0.0;
  status_msg.lateral_velocity = 0.0;
  status_msg.steering_angle = phi_i;
  status_msg.x_linear_vel = 0.0;
  status_msg.y_linear_vel = 0.0;
  status_msg.motion_radius = 0.0;

  status_publisher_.publish(status_msg);

  last_time_ = current_time_;
}

double LimonROSMessenger::ConvertInnerAngleToCentral(double angle) {
  double phi = 0;
  double phi_i = angle;
  if (phi_i > steer_angle_tolerance) {
    double r = l / std::tan(phi_i) + w / 2;
    phi = std::atan(l / r);
  } else if (phi_i < -steer_angle_tolerance) {
    double r = l / std::tan(-phi) + w / 2;
    phi = std::atan(l / r);
    phi = -phi;
  }
  return phi;
}
double LimonROSMessenger::ConvertCentralAngleToInner(double angle) {
  double phi = angle;
  double phi_i = 0.0;
  if (phi > steer_angle_tolerance) {
    phi_i =
        std::atan(l * std::sin(phi) / (l * std::cos(phi) - w * std::sin(phi)));
  } else if (phi < -steer_angle_tolerance) {
    phi = -phi;
    phi_i =
        std::atan(l * std::sin(phi) / (l * std::cos(phi) - w * std::sin(phi)));
    phi_i = -phi_i;
  }
  return phi_i;
}