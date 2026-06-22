#ifndef NRC_JOB_OPERATE_H
#define NRC_JOB_OPERATE_H

#include "../parameter/nrc_define.h"

#include <string>

Result job_get_current_file(SOCKETFD socketFd, std::string& programOut);
Result job_run(SOCKETFD socketFd, const std::string& programName);
Result job_pause(SOCKETFD socketFd);
Result job_continue(SOCKETFD socketFd);
Result job_stop(SOCKETFD socketFd);

#endif // NRC_JOB_OPERATE_H
