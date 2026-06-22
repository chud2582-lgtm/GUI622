#ifndef ROBOT_SDK_CPP_ROBOT_ARM_ACTIONS_H
#define ROBOT_SDK_CPP_ROBOT_ARM_ACTIONS_H

#include "parameter/nrc_define.h"
#include "robot_adapter.h"

#include <string>

namespace robot_arm_actions {

SOCKETFD connect(RobotBrand brand, const std::string& ip, const std::string& port);
std::string last_connect_error(RobotBrand brand);
Result disconnect(RobotBrand brand, SOCKETFD socketFd);
Result enable(RobotBrand brand, SOCKETFD socketFd);
Result start_job(RobotBrand brand, SOCKETFD socketFd, const std::string& programName);
Result pause_job(RobotBrand brand, SOCKETFD socketFd);
Result continue_job(RobotBrand brand, SOCKETFD socketFd);
Result stop_job(RobotBrand brand, SOCKETFD socketFd);
Result reset_job(RobotBrand brand, SOCKETFD socketFd, const std::string& programName);

} // namespace robot_arm_actions

#endif // ROBOT_SDK_CPP_ROBOT_ARM_ACTIONS_H
