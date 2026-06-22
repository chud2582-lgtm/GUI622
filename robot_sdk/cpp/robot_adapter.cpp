#include "robot_adapter.h"
#include "robot_arm_actions.h"
#include "backend_bridge.h"

#include "interface/nrc_interface.h"
#include "interface/nrc_io.h"
#include "interface/nrc_job_operate.h"

#include <algorithm>
#include <cctype>
#include <mutex>

namespace {

std::mutex g_brandMutex;
RobotBrand g_brand = RobotBrand::Huichuan;

std::string normalizedBrand(std::string name)
{
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char ch) {
                   return std::isspace(ch) != 0;
               }),
               name.end());
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return name;
}

bool useYinglian()
{
    return robot_brand() == RobotBrand::Yinglian;
}

} // namespace

void set_robot_brand(RobotBrand brand)
{
    std::lock_guard<std::mutex> lock(g_brandMutex);
    g_brand = brand;
}

RobotBrand robot_brand()
{
    std::lock_guard<std::mutex> lock(g_brandMutex);
    return g_brand;
}

const char* robot_brand_name(RobotBrand brand)
{
    return brand == RobotBrand::Yinglian ? "yinglian" : "huichuan";
}

RobotBrand robot_brand_from_name(const std::string& name)
{
    const std::string normalized = normalizedBrand(name);
    if (normalized == "yinglian" || normalized == "yl" || normalized == "ying") {
        return RobotBrand::Yinglian;
    }
    return RobotBrand::Huichuan;
}

SOCKETFD robot_connect_robot(const std::string& ip, const std::string& port)
{
    return robot_arm_actions::connect(robot_brand(), ip, port);
}

Result robot_disconnect_robot(SOCKETFD socketFd)
{
    return robot_arm_actions::disconnect(robot_brand(), socketFd);
}

Result robot_get_connection_status(SOCKETFD socketFd)
{
    return useYinglian()
        ? robot_backend::yinglian::get_connection_status(socketFd)
        : robot_backend::huichuan::get_connection_status(socketFd);
}

Result robot_get_servo_state(SOCKETFD socketFd, int& state)
{
    return useYinglian()
        ? robot_backend::yinglian::get_servo_state(socketFd, state)
        : robot_backend::huichuan::get_servo_state(socketFd, state);
}

Result robot_set_servo_state(SOCKETFD socketFd, int state)
{
    return useYinglian()
        ? robot_backend::yinglian::set_servo_state(socketFd, state)
        : robot_backend::huichuan::set_servo_state(socketFd, state);
}

Result robot_set_servo_poweron(SOCKETFD socketFd)
{
    return robot_arm_actions::enable(robot_brand(), socketFd);
}

Result robot_set_servo_poweroff(SOCKETFD socketFd)
{
    return useYinglian()
        ? robot_backend::yinglian::set_servo_poweroff(socketFd)
        : robot_backend::huichuan::set_servo_poweroff(socketFd);
}

Result robot_clear_error(SOCKETFD socketFd)
{
    return useYinglian()
        ? robot_backend::yinglian::clear_error(socketFd)
        : robot_backend::huichuan::clear_error(socketFd);
}

Result robot_set_current_mode(SOCKETFD socketFd, int mode)
{
    return useYinglian()
        ? robot_backend::yinglian::set_current_mode(socketFd, mode)
        : robot_backend::huichuan::set_current_mode(socketFd, mode);
}

Result robot_get_current_mode(SOCKETFD socketFd, int& mode)
{
    return useYinglian()
        ? robot_backend::yinglian::get_current_mode(socketFd, mode)
        : robot_backend::huichuan::get_current_mode(socketFd, mode);
}

Result robot_get_robot_running_state(SOCKETFD socketFd, int& state)
{
    return useYinglian()
        ? robot_backend::yinglian::get_robot_running_state(socketFd, state)
        : robot_backend::huichuan::get_robot_running_state(socketFd, state);
}

Result robot_get_robot_type(SOCKETFD socketFd, int& type)
{
    return useYinglian()
        ? robot_backend::yinglian::get_robot_type(socketFd, type)
        : robot_backend::huichuan::get_robot_type(socketFd, type);
}

Result robot_get_current_position(SOCKETFD socketFd, int coordType, std::vector<double>& position)
{
    return useYinglian()
        ? robot_backend::yinglian::get_current_position(socketFd, coordType, position)
        : robot_backend::huichuan::get_current_position(socketFd, coordType, position);
}

Result robot_set_digital_output(SOCKETFD socketFd, int port, int value)
{
    return useYinglian()
        ? robot_backend::yinglian::set_digital_output(socketFd, port, value)
        : robot_backend::huichuan::set_digital_output(socketFd, port, value);
}

Result robot_get_digital_input(SOCKETFD socketFd, int port, int& value)
{
    return useYinglian()
        ? robot_backend::yinglian::get_digital_input(socketFd, port, value)
        : robot_backend::huichuan::get_digital_input(socketFd, port, value);
}

Result robot_get_digital_output(SOCKETFD socketFd, int port, int& value)
{
    return useYinglian()
        ? robot_backend::yinglian::get_digital_output(socketFd, port, value)
        : robot_backend::huichuan::get_digital_output(socketFd, port, value);
}

Result robot_set_global_variant(SOCKETFD socketFd, const std::string& name, double value)
{
    return useYinglian()
        ? robot_backend::yinglian::set_global_variant(socketFd, name, value)
        : robot_backend::huichuan::set_global_variant(socketFd, name, value);
}

std::string robot_last_global_variant_error()
{
    return useYinglian()
        ? std::string()
        : robot_backend::huichuan::last_global_variant_error();
}

std::string robot_login_admin_user()
{
    return useYinglian()
        ? std::string()
        : robot_backend::huichuan::login_admin_user();
}

Result robot_get_global_variant(SOCKETFD socketFd, const std::string& name, double& value)
{
    return useYinglian()
        ? robot_backend::yinglian::get_global_variant(socketFd, name, value)
        : robot_backend::huichuan::get_global_variant(socketFd, name, value);
}

Result robot_job_get_current_file(SOCKETFD socketFd, std::string& programOut)
{
    return useYinglian()
        ? robot_backend::yinglian::job_get_current_file(socketFd, programOut)
        : robot_backend::huichuan::job_get_current_file(socketFd, programOut);
}

Result robot_job_run(SOCKETFD socketFd, const std::string& programName)
{
    return robot_arm_actions::start_job(robot_brand(), socketFd, programName);
}

Result robot_job_pause(SOCKETFD socketFd)
{
    return robot_arm_actions::pause_job(robot_brand(), socketFd);
}

Result robot_job_continue(SOCKETFD socketFd)
{
    return robot_arm_actions::continue_job(robot_brand(), socketFd);
}

Result robot_job_stop(SOCKETFD socketFd)
{
    return robot_arm_actions::stop_job(robot_brand(), socketFd);
}

