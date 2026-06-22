#ifndef NRC_JOB_OPERATE_H
#define NRC_JOB_OPERATE_H

#include <string>
#include <vector>

#include "cpp/parameter/nrc_define.h"

struct JobStartCheckItem
{
    std::string name;
    Result result = EXCEPTION;
    std::string detail;
};

Result job_run_times(SOCKETFD socketFd, int times);
Result job_get_current_task_program_path(SOCKETFD socketFd, std::string& pathOut);
Result job_diagnose_start_conditions(SOCKETFD socketFd, std::vector<JobStartCheckItem>& itemsOut);
Result job_run(SOCKETFD socketFd, const std::string& name);
Result job_pause(SOCKETFD socketFd);
Result job_continue(SOCKETFD socketFd);
Result job_reset(SOCKETFD socketFd);
Result job_stop(SOCKETFD socketFd);

#endif // NRC_JOB_OPERATE_H
