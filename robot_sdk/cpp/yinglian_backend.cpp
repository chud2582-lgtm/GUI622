#include "backend_bridge.h"

#include <mutex>
#include <string>
#include <vector>

// Yinglian's SDK uses the original nrc_* C++ symbols. Keep these declarations
// local to this backend so the GUI-facing adapter can use robot_* names.
extern SOCKETFD connect_robot(const std::string& ip, const std::string& port);
extern Result disconnect_robot(SOCKETFD socketFd);
extern Result get_connection_status(SOCKETFD socketFd);
extern Result get_servo_state(SOCKETFD socketFd, int& state);
extern Result set_servo_state(SOCKETFD socketFd, int state);
extern Result set_servo_poweron(SOCKETFD socketFd);
extern Result set_servo_poweroff(SOCKETFD socketFd);
extern Result clear_error(SOCKETFD socketFd);
extern Result set_current_mode(SOCKETFD socketFd, int mode);
extern Result get_current_mode(SOCKETFD socketFd, int& mode);
extern Result get_robot_running_state(SOCKETFD socketFd, int& state);
extern Result get_robot_type(SOCKETFD socketFd, int& type);
extern Result get_current_position(SOCKETFD socketFd, int coordType, std::vector<double>& position);
extern Result set_digital_output(SOCKETFD socketFd, int port, int value);
extern Result get_digital_input(SOCKETFD socketFd, std::vector<int>& values);
extern Result get_digital_output(SOCKETFD socketFd, std::vector<int>& values);
extern Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value);
extern Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& value);
extern Result job_get_current_file(SOCKETFD socketFd, std::string& programOut);
extern Result job_run(SOCKETFD socketFd, const std::string& programName);
extern Result job_pause(SOCKETFD socketFd);
extern Result job_continue(SOCKETFD socketFd);
extern Result job_stop(SOCKETFD socketFd);

namespace robot_backend::yinglian {
namespace {

std::mutex g_sdkMutex;
bool g_connected = false;

bool isValidSocket(SOCKETFD socketFd)
{
    return socketFd >= 0 && g_connected;
}

Result requireConnection(SOCKETFD socketFd)
{
    return isValidSocket(socketFd) ? SUCCESS : DISCONNECT;
}

} // namespace

SOCKETFD connect_robot(const std::string& ip, const std::string& port)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (g_connected) {
        ::disconnect_robot(0);
        g_connected = false;
    }
    const SOCKETFD fd = ::connect_robot(ip, port);
    if (fd >= 0) {
        g_connected = true;
    }
    return fd;
}

Result disconnect_robot(SOCKETFD socketFd)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (!isValidSocket(socketFd)) return DISCONNECT;
    const Result result = ::disconnect_robot(socketFd);
    g_connected = false;
    return result;
}

Result get_connection_status(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::get_connection_status(socketFd); }
Result get_servo_state(SOCKETFD socketFd, int& state) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::get_servo_state(socketFd, state); }
Result set_servo_state(SOCKETFD socketFd, int state) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::set_servo_state(socketFd, state); }
Result set_servo_poweron(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::set_servo_poweron(socketFd); }
Result set_servo_poweroff(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::set_servo_poweroff(socketFd); }
Result clear_error(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::clear_error(socketFd); }
Result set_current_mode(SOCKETFD socketFd, int mode) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::set_current_mode(socketFd, mode); }
Result get_current_mode(SOCKETFD socketFd, int& mode) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::get_current_mode(socketFd, mode); }
Result get_robot_running_state(SOCKETFD socketFd, int& state) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::get_robot_running_state(socketFd, state); }
Result get_robot_type(SOCKETFD socketFd, int& type) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::get_robot_type(socketFd, type); }
Result get_current_position(SOCKETFD socketFd, int coordType, std::vector<double>& position) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::get_current_position(socketFd, coordType, position); }
Result set_digital_output(SOCKETFD socketFd, int port, int value) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::set_digital_output(socketFd, port, value); }

Result get_digital_input(SOCKETFD socketFd, int port, int& value)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(socketFd) != SUCCESS) return DISCONNECT;
    std::vector<int> values;
    const Result result = ::get_digital_input(socketFd, values);
    if (result == SUCCESS && port >= 1 && static_cast<size_t>(port) <= values.size()) {
        value = values.at(static_cast<size_t>(port - 1));
    }
    return result;
}

Result get_digital_output(SOCKETFD socketFd, int port, int& value)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(socketFd) != SUCCESS) return DISCONNECT;
    std::vector<int> values;
    const Result result = ::get_digital_output(socketFd, values);
    if (result == SUCCESS && port >= 1 && static_cast<size_t>(port) <= values.size()) {
        value = values.at(static_cast<size_t>(port - 1));
    }
    return result;
}

Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::set_global_variant(socketFd, name, value); }
Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& value) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::get_global_variant(socketFd, name, value); }
Result job_get_current_file(SOCKETFD socketFd, std::string& programOut) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::job_get_current_file(socketFd, programOut); }
Result job_run(SOCKETFD socketFd, const std::string& programName) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::job_run(socketFd, programName); }
Result job_pause(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::job_pause(socketFd); }
Result job_continue(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::job_continue(socketFd); }
Result job_stop(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return ::job_stop(socketFd); }

} // namespace robot_backend::yinglian
