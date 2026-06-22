#ifndef NRC_INTERFACE_H
#define NRC_INTERFACE_H

#include <string>
#include <vector>

#include "cpp/parameter/nrc_define.h"
#include "cpp/parameter/nrc_interface_parameter.h"

SOCKETFD connect_robot(const std::string& ip, const std::string& port);
Result disconnect_robot(SOCKETFD socketFd);

Result set_servo_state(SOCKETFD socketFd, int state);
Result set_servo_poweron(SOCKETFD socketFd);
Result set_servo_poweroff(SOCKETFD socketFd);
Result clear_error(SOCKETFD socketFd);

Result set_current_mode(SOCKETFD socketFd, int mode);
Result set_current_coord(SOCKETFD socketFd, int coord);
Result set_speed(SOCKETFD socketFd, int speed);

Result robot_start_jogging(SOCKETFD socketFd, int axis, bool positiveDirection);
Result robot_stop_jogging(SOCKETFD socketFd, int axis);

Result robot_movej(SOCKETFD socketFd, const MoveCmd& cmd);
Result robot_movel(SOCKETFD socketFd, const MoveCmd& cmd);
Result robot_go_home(SOCKETFD socketFd);
Result robot_go_to_reset_position(SOCKETFD socketFd);

Result get_current_position(SOCKETFD socketFd, int positionType, std::vector<double>& poseOut);
Result get_servo_state(SOCKETFD socketFd, int& stateOut);
Result get_robot_running_state(SOCKETFD socketFd, int& stateOut);
Result get_robot_type(SOCKETFD socketFd, int& typeOut);

Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value);
Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& valueOut);

#endif // NRC_INTERFACE_H
