#include "robot_arm_actions_backend.h"

#include "backend_bridge.h"

namespace robot_arm_brand::huichuan {

SOCKETFD connect(const std::string& ip, const std::string& port)
{
    return robot_backend::huichuan::connect_robot(ip, port);
}

std::string last_connect_error()
{
    return robot_backend::huichuan::last_connect_error();
}

Result disconnect(SOCKETFD socketFd)
{
    return robot_backend::huichuan::disconnect_robot(socketFd);
}

Result enable(SOCKETFD socketFd)
{
    return robot_backend::huichuan::set_servo_poweron(socketFd);
}

Result start_job(SOCKETFD socketFd, const std::string& programName)
{
    std::string activeProgram;
    const Result currentFileResult = robot_backend::huichuan::job_get_current_file(socketFd, activeProgram);
    if (currentFileResult != SUCCESS) {
        return currentFileResult;
    }
    return robot_backend::huichuan::job_run(socketFd, activeProgram);
}

Result pause_job(SOCKETFD socketFd)
{
    return robot_backend::huichuan::job_pause(socketFd);
}

Result continue_job(SOCKETFD socketFd)
{
    return robot_backend::huichuan::job_continue(socketFd);
}

Result stop_job(SOCKETFD socketFd)
{
    return robot_backend::huichuan::job_stop(socketFd);
}

Result reset_job(SOCKETFD socketFd, const std::string&)
{
    return robot_backend::huichuan::job_stop(socketFd);
}

} // namespace robot_arm_brand::huichuan
