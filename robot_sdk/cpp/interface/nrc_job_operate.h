#ifndef ROBOT_SDK_CPP_INTERFACE_NRC_JOB_OPERATE_H
#define ROBOT_SDK_CPP_INTERFACE_NRC_JOB_OPERATE_H

#include "../parameter/nrc_define.h"

#include <string>

Result robot_job_get_current_file(SOCKETFD socketFd, std::string& programOut);
Result robot_job_run(SOCKETFD socketFd, const std::string& programName);
Result robot_job_pause(SOCKETFD socketFd);
Result robot_job_continue(SOCKETFD socketFd);
Result robot_job_stop(SOCKETFD socketFd);

#endif // ROBOT_SDK_CPP_INTERFACE_NRC_JOB_OPERATE_H
