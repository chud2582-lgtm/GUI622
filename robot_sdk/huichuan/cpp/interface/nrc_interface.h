#ifndef NRC_INTERFACE_H
#define NRC_INTERFACE_H

#include "../parameter/nrc_define.h"

#include <string>
#include <vector>

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

Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value);
Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& value);

#endif // NRC_INTERFACE_H
