#ifndef ROBOT_SDK_CPP_INTERFACE_NRC_INTERFACE_H
#define ROBOT_SDK_CPP_INTERFACE_NRC_INTERFACE_H

#include "../parameter/nrc_define.h"

#include <string>
#include <vector>

SOCKETFD robot_connect_robot(const std::string& ip, const std::string& port);
Result robot_disconnect_robot(SOCKETFD socketFd);
Result robot_get_connection_status(SOCKETFD socketFd);
Result robot_get_servo_state(SOCKETFD socketFd, int& state);
Result robot_set_servo_state(SOCKETFD socketFd, int state);
Result robot_set_servo_poweron(SOCKETFD socketFd);
Result robot_set_servo_poweroff(SOCKETFD socketFd);
Result robot_clear_error(SOCKETFD socketFd);
Result robot_set_current_mode(SOCKETFD socketFd, int mode);
Result robot_get_current_mode(SOCKETFD socketFd, int& mode);
Result robot_get_robot_running_state(SOCKETFD socketFd, int& state);
Result robot_get_robot_type(SOCKETFD socketFd, int& type);
Result robot_get_current_position(SOCKETFD socketFd, int coordType, std::vector<double>& position);
Result robot_set_global_variant(SOCKETFD socketFd, const std::string& name, double value);
std::string robot_last_global_variant_error();
std::string robot_login_admin_user();
Result robot_get_global_variant(SOCKETFD socketFd, const std::string& name, double& value);

#endif // ROBOT_SDK_CPP_INTERFACE_NRC_INTERFACE_H
