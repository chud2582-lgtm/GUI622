#include "robot_arm_actions.h"

#include "robot_arm_actions_backend.h"

namespace robot_arm_actions {

SOCKETFD connect(RobotBrand brand, const std::string& ip, const std::string& port)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::connect(ip, port)
        : robot_arm_brand::huichuan::connect(ip, port);
}

std::string last_connect_error(RobotBrand brand)
{
    return brand == RobotBrand::Huichuan
        ? robot_arm_brand::huichuan::last_connect_error()
        : std::string();
}

Result disconnect(RobotBrand brand, SOCKETFD socketFd)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::disconnect(socketFd)
        : robot_arm_brand::huichuan::disconnect(socketFd);
}

Result enable(RobotBrand brand, SOCKETFD socketFd)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::enable(socketFd)
        : robot_arm_brand::huichuan::enable(socketFd);
}

Result start_job(RobotBrand brand, SOCKETFD socketFd, const std::string& programName)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::start_job(socketFd, programName)
        : robot_arm_brand::huichuan::start_job(socketFd, programName);
}

Result pause_job(RobotBrand brand, SOCKETFD socketFd)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::pause_job(socketFd)
        : robot_arm_brand::huichuan::pause_job(socketFd);
}

Result continue_job(RobotBrand brand, SOCKETFD socketFd)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::continue_job(socketFd)
        : robot_arm_brand::huichuan::continue_job(socketFd);
}

Result stop_job(RobotBrand brand, SOCKETFD socketFd)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::stop_job(socketFd)
        : robot_arm_brand::huichuan::stop_job(socketFd);
}

Result reset_job(RobotBrand brand, SOCKETFD socketFd, const std::string& programName)
{
    return brand == RobotBrand::Yinglian
        ? robot_arm_brand::yinglian::reset_job(socketFd, programName)
        : robot_arm_brand::huichuan::reset_job(socketFd, programName);
}

} // namespace robot_arm_actions
