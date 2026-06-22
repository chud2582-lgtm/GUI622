#ifndef ROBOT_SDK_CPP_ROBOT_ARM_ACTIONS_BACKEND_H
#define ROBOT_SDK_CPP_ROBOT_ARM_ACTIONS_BACKEND_H

#include "parameter/nrc_define.h"

#include <string>

namespace robot_arm_brand {

namespace huichuan {
SOCKETFD connect(const std::string& ip, const std::string& port);
std::string last_connect_error();
Result disconnect(SOCKETFD socketFd);
Result enable(SOCKETFD socketFd);
Result start_job(SOCKETFD socketFd, const std::string& programName);
Result pause_job(SOCKETFD socketFd);
Result continue_job(SOCKETFD socketFd);
Result stop_job(SOCKETFD socketFd);
Result reset_job(SOCKETFD socketFd, const std::string& programName);
} // namespace huichuan

namespace yinglian {
SOCKETFD connect(const std::string& ip, const std::string& port);
Result disconnect(SOCKETFD socketFd);
Result enable(SOCKETFD socketFd);
Result start_job(SOCKETFD socketFd, const std::string& programName);
Result pause_job(SOCKETFD socketFd);
Result continue_job(SOCKETFD socketFd);
Result stop_job(SOCKETFD socketFd);
Result reset_job(SOCKETFD socketFd, const std::string& programName);
} // namespace yinglian

} // namespace robot_arm_brand

#endif // ROBOT_SDK_CPP_ROBOT_ARM_ACTIONS_BACKEND_H
