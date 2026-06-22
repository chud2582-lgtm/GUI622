#ifndef ROBOT_SDK_CPP_BACKEND_BRIDGE_H
#define ROBOT_SDK_CPP_BACKEND_BRIDGE_H

#include "parameter/nrc_define.h"

#include <string>
#include <vector>

namespace robot_backend {

namespace huichuan {
SOCKETFD connect_robot(const std::string& ip, const std::string& port);
std::string last_connect_error();
std::string login_admin_user();
Result disconnect_robot(SOCKETFD socketFd);
Result get_connection_status(SOCKETFD socketFd);
Result get_servo_state(SOCKETFD socketFd, int& state);
Result set_servo_state(SOCKETFD socketFd, int state);
Result set_servo_poweron(SOCKETFD socketFd);
Result set_servo_poweroff(SOCKETFD socketFd);
Result clear_error(SOCKETFD socketFd);
Result set_current_mode(SOCKETFD socketFd, int mode);
Result get_current_mode(SOCKETFD socketFd, int& mode);
Result get_robot_running_state(SOCKETFD socketFd, int& state);
Result get_robot_type(SOCKETFD socketFd, int& type);
Result get_current_position(SOCKETFD socketFd, int coordType, std::vector<double>& position);
Result set_digital_output(SOCKETFD socketFd, int port, int value);
Result get_digital_input(SOCKETFD socketFd, int port, int& value);
Result get_digital_output(SOCKETFD socketFd, int port, int& value);
Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value);
std::string last_global_variant_error();
Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& value);
Result job_get_current_file(SOCKETFD socketFd, std::string& programOut);
Result job_run(SOCKETFD socketFd, const std::string& programName);
Result job_pause(SOCKETFD socketFd);
Result job_continue(SOCKETFD socketFd);
Result job_stop(SOCKETFD socketFd);
} // namespace huichuan

namespace yinglian {
SOCKETFD connect_robot(const std::string& ip, const std::string& port);
Result disconnect_robot(SOCKETFD socketFd);
Result get_connection_status(SOCKETFD socketFd);
Result get_servo_state(SOCKETFD socketFd, int& state);
Result set_servo_state(SOCKETFD socketFd, int state);
Result set_servo_poweron(SOCKETFD socketFd);
Result set_servo_poweroff(SOCKETFD socketFd);
Result clear_error(SOCKETFD socketFd);
Result set_current_mode(SOCKETFD socketFd, int mode);
Result get_current_mode(SOCKETFD socketFd, int& mode);
Result get_robot_running_state(SOCKETFD socketFd, int& state);
Result get_robot_type(SOCKETFD socketFd, int& type);
Result get_current_position(SOCKETFD socketFd, int coordType, std::vector<double>& position);
Result set_digital_output(SOCKETFD socketFd, int port, int value);
Result get_digital_input(SOCKETFD socketFd, int port, int& value);
Result get_digital_output(SOCKETFD socketFd, int port, int& value);
Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value);
Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& value);
Result job_get_current_file(SOCKETFD socketFd, std::string& programOut);
Result job_run(SOCKETFD socketFd, const std::string& programName);
Result job_pause(SOCKETFD socketFd);
Result job_continue(SOCKETFD socketFd);
Result job_stop(SOCKETFD socketFd);
} // namespace yinglian

} // namespace robot_backend

#endif // ROBOT_SDK_CPP_BACKEND_BRIDGE_H
