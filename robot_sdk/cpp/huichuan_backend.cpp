#include "backend_bridge.h"

#include "../huichuan/IMC100API.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <regex>
#include <utility>

namespace robot_backend::huichuan {
namespace {

constexpr SOCKETFD kInvalidSocket = -1;
constexpr SOCKETFD kComId = 0;
constexpr int kConnectTimeoutSeconds = 5;

std::mutex g_sdkMutex;
bool g_connected = false;
std::string g_lastConnectError;
std::string g_lastGlobalVariantError;

Result toResult(int code)
{
    if (code == 0) return SUCCESS;
    if (code == -2) return TIMEOUT;
    if (code == -3) return PARAM_ERR;
    return EXCEPTION;
}

bool isValidSocket(SOCKETFD socketFd)
{
    return socketFd == kComId && g_connected;
}

Result requireConnection(SOCKETFD socketFd)
{
    return isValidSocket(socketFd) ? SUCCESS : DISCONNECT;
}

Result ensureControlPermit(SOCKETFD socketFd)
{
    g_lastGlobalVariantError.clear();
    const Result connectionResult = requireConnection(socketFd);
    if (connectionResult != SUCCESS) {
        g_lastGlobalVariantError = "Control permit check failed: disconnected.";
        return connectionResult;
    }

    int owner = 0;
    unsigned int ownerIp = 0;
    unsigned short ownerPort = 0;
    const int curPermitResult = IMC100_CurPermit(&owner, &ownerIp, &ownerPort, kComId);
    if (curPermitResult < 0) {
        g_lastGlobalVariantError = "IMC100_CurPermit failed. curPermitResult=" + std::to_string(curPermitResult);
        return EXCEPTION;
    }

    if (owner == 1) {
        return SUCCESS;
    }

    int acqNormal = -999999;
    int acqForce = -999999;
    if (owner == 0) {
        acqNormal = IMC100_AcqPermit(0, kComId);
        if (acqNormal >= 0) {
            return SUCCESS;
        }
        acqForce = IMC100_AcqPermit(1, kComId);
    } else {
        acqForce = IMC100_AcqPermit(1, kComId);
    }

    int ownerAfter = 0;
    unsigned int ownerIpAfter = 0;
    unsigned short ownerPortAfter = 0;
    const int curPermitAfter = IMC100_CurPermit(&ownerAfter, &ownerIpAfter, &ownerPortAfter, kComId);
    g_lastGlobalVariantError = "Control permit acquisition failed. "
                               "curPermitResult=" + std::to_string(curPermitResult)
                               + ", ownerBefore=" + std::to_string(owner)
                               + ", ownerIpBefore=" + std::to_string(ownerIp)
                               + ", ownerPortBefore=" + std::to_string(ownerPort)
                               + ", acqNormal=" + std::to_string(acqNormal)
                               + ", acqForce=" + std::to_string(acqForce)
                               + ", curPermitAfter=" + std::to_string(curPermitAfter)
                               + ", ownerAfter=" + std::to_string(ownerAfter)
                               + ", ownerIpAfter=" + std::to_string(ownerIpAfter)
                               + ", ownerPortAfter=" + std::to_string(ownerPortAfter);
    return acqForce >= 0 ? SUCCESS : OPERATION_NOT_ALLOWED;
}

bool parseIpAddress(const std::string& ip, unsigned int& ipAddr)
{
    std::array<unsigned int, 4> parts = {};
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &parts[0], &parts[1], &parts[2], &parts[3]) != 4) {
        return false;
    }
    for (unsigned int part : parts) {
        if (part > 255) return false;
    }
    ipAddr = (parts[0] << 24U) | (parts[1] << 16U) | (parts[2] << 8U) | parts[3];
    return true;
}

std::string normalizedName(std::string name)
{
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char ch) { return std::isspace(ch) != 0; }), name.end());
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return name;
}

bool parseVariableName(const std::string& name, char& type, int& index)
{
    static const std::regex pattern(R"(G?([BRID])0*([0-9]+))", std::regex::icase);
    std::smatch match;
    const std::string normalized = normalizedName(name);
    if (!std::regex_match(normalized, match, pattern)) return false;
    type = static_cast<char>(std::toupper(static_cast<unsigned char>(match[1].str()[0])));
    if (type == 'I') type = 'R';
    index = std::stoi(match[2].str());
    return index >= 0;
}

std::string initEthFailureHint(int code)
{
    switch (toResult(code)) {
    case TIMEOUT:
        return "SDK reported TIMEOUT; check controller power, network reachability, IP address, port, firewall, and network segment.";
    case PARAM_ERR:
        return "SDK reported PARAM_ERR; check whether the IP address and port are valid for this controller.";
    case EXCEPTION:
        return "SDK reported EXCEPTION; check SDK runtime files, controller state, and whether another client is already connected.";
    default:
        return "SDK reported an unsuccessful result.";
    }
}

std::string sdkFailureHint(int code)
{
    switch (code) {
    case -27:
        return "low user grade; log in with edit/admin-or-higher user level before calling write APIs.";
    case -28:
        return "permit occupied; force acquire control permit and retry.";
    default:
        return "SDK reported failure.";
    }
}

void setLastConnectError(std::string message)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    g_lastConnectError = std::move(message);
}

} // namespace

SOCKETFD connect_robot(const std::string& ip, const std::string& port)
{
    unsigned int ipAddr = 0;
    if (!parseIpAddress(ip, ipAddr)) {
        setLastConnectError("Invalid Huichuan controller IP address: " + ip);
        return kInvalidSocket;
    }
    int portValue = 0;
    try {
        portValue = std::stoi(port);
    } catch (...) {
        setLastConnectError("Invalid Huichuan controller port: " + port);
        return kInvalidSocket;
    }
    if (portValue <= 0 || portValue > 65535) {
        setLastConnectError("Invalid Huichuan controller port range: " + port + " (expected 1-65535)");
        return kInvalidSocket;
    }

    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (g_connected) {
        const int exitResult = IMC100_Exit_ETH(kComId);
        g_connected = false;
        if (exitResult != 0) {
            g_lastConnectError = "Failed to close the previous Huichuan SDK connection before reconnecting. "
                                 "IMC100_Exit_ETH returned " + std::to_string(exitResult) + ".";
            return kInvalidSocket;
        }
    }
    const int result = IMC100_Init_ETH(ipAddr, static_cast<unsigned short>(portValue), kConnectTimeoutSeconds, kComId);
    if (result != 0) {
        g_lastConnectError = "IMC100_Init_ETH failed. ip=" + ip + ", port=" + port
                             + ", timeoutSeconds=" + std::to_string(kConnectTimeoutSeconds)
                             + ", comId=" + std::to_string(kComId)
                             + ", sdkCode=" + std::to_string(result)
                             + ", reason=" + initEthFailureHint(result);
        return kInvalidSocket;
    }
    g_lastConnectError.clear();
    g_connected = true;
    return kComId;
}

std::string last_connect_error()
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    return g_lastConnectError;
}

std::string login_admin_user()
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(kComId) != SUCCESS) {
        return "Huichuan admin login skipped: controller is not connected.";
    }

    int ownerBefore = 0;
    unsigned int ownerIpBefore = 0;
    unsigned short ownerPortBefore = 0;
    const int permitBeforeResult = IMC100_CurPermit(&ownerBefore, &ownerIpBefore, &ownerPortBefore, kComId);

    const Result permitResult = ensureControlPermit(kComId);
    const std::string permitDetail = g_lastGlobalVariantError;

    int ownerAfterPermit = 0;
    unsigned int ownerIpAfterPermit = 0;
    unsigned short ownerPortAfterPermit = 0;
    const int permitAfterResult = IMC100_CurPermit(&ownerAfterPermit, &ownerIpAfterPermit, &ownerPortAfterPermit, kComId);

    int beforeType = 0;
    const int beforeResult = IMC100_CurUserType(&beforeType, kComId);

    char password[8] = {};
    std::memcpy(password, "000000", 6);
    constexpr int kAdminUserType = 2;
    const int loginResult = permitResult == SUCCESS
        ? IMC100_UserLogin(kAdminUserType, password, kComId)
        : -24;

    int afterType = 0;
    const int afterResult = IMC100_CurUserType(&afterType, kComId);

    return "Huichuan admin login: permitBeforeResult=" + std::to_string(permitBeforeResult)
           + ", ownerBefore=" + std::to_string(ownerBefore)
           + ", permitResult=" + std::to_string(permitResult)
           + ", permitAfterResult=" + std::to_string(permitAfterResult)
           + ", ownerAfterPermit=" + std::to_string(ownerAfterPermit)
           + (permitDetail.empty() ? std::string() : ", permitDetail={" + permitDetail + "}")
           + ", beforeResult=" + std::to_string(beforeResult)
           + ", beforeUserType=" + std::to_string(beforeType)
           + ", loginType=" + std::to_string(kAdminUserType)
           + ", loginResult=" + std::to_string(loginResult)
           + ", afterResult=" + std::to_string(afterResult)
           + ", afterUserType=" + std::to_string(afterType);
}

std::string last_global_variant_error()
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    return g_lastGlobalVariantError;
}

Result disconnect_robot(SOCKETFD socketFd)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (!isValidSocket(socketFd)) return DISCONNECT;
    const Result result = toResult(IMC100_Exit_ETH(kComId));
    g_connected = false;
    return result;
}

Result get_connection_status(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); return requireConnection(socketFd); }
Result get_servo_state(SOCKETFD socketFd, int& state)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(socketFd) != SUCCESS) return DISCONNECT;
    int motorState = 0;
    const Result result = toResult(IMC100_Get_MotorSts(&motorState, kComId));
    if (result == SUCCESS) state = motorState != 0 ? 3 : 0;
    return result;
}
Result set_servo_state(SOCKETFD socketFd, int state) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_MotorEnable(state != 0 ? 1 : 0, kComId)); }
Result set_servo_poweron(SOCKETFD socketFd) { return set_servo_state(socketFd, 1); }
Result set_servo_poweroff(SOCKETFD socketFd) { return set_servo_state(socketFd, 0); }
Result clear_error(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_ResetErr(kComId)); }
Result set_current_mode(SOCKETFD socketFd, int mode) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_Set_Mode(mode, kComId)); }
Result get_current_mode(SOCKETFD socketFd, int& mode) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_Get_Mode(&mode, kComId)); }
Result get_robot_running_state(SOCKETFD socketFd, int& state) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_Get_MotionSts(&state, kComId)); }
Result get_robot_type(SOCKETFD socketFd, int& type)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(socketFd) != SUCCESS) return DISCONNECT;
    int axisNum = 0;
    const Result result = toResult(IMC100_Get_RobotAxisNum(&axisNum, kComId));
    if (result == SUCCESS) type = axisNum == 7 ? 12 : axisNum == 6 ? 1 : axisNum;
    return result;
}
Result get_current_position(SOCKETFD socketFd, int coordType, std::vector<double>& position)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(socketFd) != SUCCESS) return DISCONNECT;
    if (coordType > 0) IMC100_Set_ActiveMechUnitPosFormat(coordType, kComId);
    ROB_POS pos = {};
    const Result result = toResult(IMC100_Get_RobPosHere(&pos, kComId));
    if (result == SUCCESS) position.assign(pos.RPosData, pos.RPosData + 6);
    else position.clear();
    return result;
}
Result set_digital_output(SOCKETFD socketFd, int port, int value) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_Set_DO(port, value != 0 ? 1 : 0, kComId)); }
Result get_digital_input(SOCKETFD socketFd, int port, int& value) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_Get_DI(port, &value, kComId)); }
Result get_digital_output(SOCKETFD socketFd, int port, int& value) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_Get_DO(port, &value, kComId)); }
Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    const Result permitResult = ensureControlPermit(socketFd);
    if (permitResult != SUCCESS) {
        g_lastGlobalVariantError = "Global variant write blocked before IMC100_Set. name=" + name
                                   + ", permitResult=" + std::to_string(permitResult)
                                   + ", detail={" + g_lastGlobalVariantError + "}";
        return permitResult;
    }
    char type = '\0';
    int index = 0;
    if (!parseVariableName(name, type, index)) {
        g_lastGlobalVariantError = "Invalid Huichuan global variant name: " + name;
        return PARAM_ERR;
    }
    int sdkResult = 0;
    switch (type) {
    case 'B':
        sdkResult = IMC100_Set_B(index, std::abs(value) > 0.000001 ? 1 : 0, kComId);
        break;
    case 'R':
        sdkResult = IMC100_Set_R(index, static_cast<int>(std::llround(value)), kComId);
        break;
    case 'D':
        sdkResult = IMC100_Set_D(index, value, kComId);
        break;
    default:
        g_lastGlobalVariantError = "Unsupported Huichuan global variant type for name: " + name;
        return PARAM_ERR;
    }
    if (sdkResult != 0) {
        int userType = 0;
        const int userTypeResult = IMC100_CurUserType(&userType, kComId);
        g_lastGlobalVariantError = "IMC100_Set failed. name=" + name
                                   + ", type=" + std::string(1, type)
                                   + ", index=" + std::to_string(index)
                                   + ", sdkCode=" + std::to_string(sdkResult)
                                   + ", value=" + std::to_string(value)
                                   + ", curUserTypeResult=" + std::to_string(userTypeResult)
                                   + ", userType=" + std::to_string(userType)
                                   + ", hint=" + sdkFailureHint(sdkResult);
        return toResult(sdkResult);
    }
    g_lastGlobalVariantError.clear();
    return SUCCESS;
}
Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& value)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(socketFd) != SUCCESS) return DISCONNECT;
    char type = '\0';
    int index = 0;
    if (!parseVariableName(name, type, index)) return PARAM_ERR;
    switch (type) {
    case 'B': { int intValue = 0; const Result result = toResult(IMC100_Get_B(index, &intValue, kComId)); if (result == SUCCESS) value = intValue; return result; }
    case 'R': { int intValue = 0; const Result result = toResult(IMC100_Get_R(index, &intValue, kComId)); if (result == SUCCESS) value = intValue; return result; }
    case 'D': return toResult(IMC100_Get_D(index, &value, kComId));
    default: return PARAM_ERR;
    }
}
Result job_get_current_file(SOCKETFD socketFd, std::string& programOut)
{
    std::lock_guard<std::mutex> lock(g_sdkMutex);
    if (requireConnection(socketFd) != SUCCESS) return DISCONNECT;
    Result lastResult = EXCEPTION;
    for (int taskId : {0, 1}) {
        char path[128] = {};
        lastResult = toResult(IMC100_Get_TaskPrgPath(taskId, path, kComId));
        if (lastResult == SUCCESS && path[0] != '\0') {
            programOut.assign(path);
            return SUCCESS;
        }
    }
    programOut.clear();
    return lastResult;
}
Result job_run(SOCKETFD socketFd, const std::string&) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_PrgCtrl(1, kComId)); }
Result job_pause(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_PrgCtrl(2, kComId)); }
Result job_continue(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_PrgCtrl(3, kComId)); }
Result job_stop(SOCKETFD socketFd) { std::lock_guard<std::mutex> lock(g_sdkMutex); if (requireConnection(socketFd) != SUCCESS) return DISCONNECT; return toResult(IMC100_PrgCtrl(4, kComId)); }

} // namespace robot_backend::huichuan
