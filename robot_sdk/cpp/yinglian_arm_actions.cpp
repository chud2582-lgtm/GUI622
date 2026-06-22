#include "robot_arm_actions_backend.h"

#include "backend_bridge.h"

namespace robot_arm_brand::yinglian {

SOCKETFD connect(const std::string& ip, const std::string& port)
{
    return robot_backend::yinglian::connect_robot(ip, port);
}

Result disconnect(SOCKETFD socketFd)
{
    return robot_backend::yinglian::disconnect_robot(socketFd);
}

Result enable(SOCKETFD socketFd)
{
    return robot_backend::yinglian::set_servo_poweron(socketFd);
}

Result start_job(SOCKETFD socketFd, const std::string& programName)
{
    return robot_backend::yinglian::job_run(socketFd, programName);
}

Result pause_job(SOCKETFD socketFd)
{
    return robot_backend::yinglian::job_pause(socketFd);
}

Result continue_job(SOCKETFD socketFd)
{
    return robot_backend::yinglian::job_continue(socketFd);
}

Result stop_job(SOCKETFD socketFd)
{
    return robot_backend::yinglian::job_stop(socketFd);
}

Result reset_job(SOCKETFD socketFd, const std::string&)
{
    return robot_backend::yinglian::job_stop(socketFd);
}

} // namespace robot_arm_brand::yinglian
