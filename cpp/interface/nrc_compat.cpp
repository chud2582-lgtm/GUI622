#include "cpp/interface/nrc_interface.h"

#include "cpp/interface/nrc_io.h"
#include "cpp/interface/nrc_job_operate.h"

#include "robot_sdk/IMC100API.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr SOCKETFD kDefaultComId = 0;
constexpr int kMainTaskId = 0;
constexpr int kHomeIndex = 0;
constexpr int kSdkTimeoutSeconds = 5;

using FnInitEth = decltype(&::IMC100_Init_ETH);
using FnExitEth = decltype(&::IMC100_Exit_ETH);
using FnCurPermit = decltype(&::IMC100_CurPermit);
using FnAcqPermit = decltype(&::IMC100_AcqPermit);
using FnRemovePermit = decltype(&::IMC100_RemovePermit);
using FnGetEStopSts = decltype(&::IMC100_Get_EStopSts);
using FnGetSysErr = decltype(&::IMC100_Get_SysErr);
using FnGetMotorSts = decltype(&::IMC100_Get_MotorSts);
using FnGetMotionSts = decltype(&::IMC100_Get_MotionSts);
using FnGetRobotType = decltype(&::IMC100_Get_RobotType);
using FnGetRobotAxisNum = decltype(&::IMC100_Get_RobotAxisNum);
using FnMotorEnable = decltype(&::IMC100_MotorEnable);
using FnResetErr = decltype(&::IMC100_ResetErr);
using FnSetMode = decltype(&::IMC100_Set_Mode);
using FnPrgCtrl = decltype(&::IMC100_PrgCtrl);
using FnBackStartLine = decltype(&::IMC100_BackStartLine);
using FnSetVel = decltype(&::IMC100_Set_Vel);
using FnSetAcc = decltype(&::IMC100_Set_Acc);
using FnSetAccRamp = decltype(&::IMC100_Set_AccRamp);
using FnDsMode = decltype(&::IMC100_DsMode);
using FnSetCoordType = decltype(&::IMC100_Set_CoordType);
using FnAxisJog = decltype(&::IMC100_AxisJog);
using FnHome = decltype(&::IMC100_Home);
using FnMovJRobPos = decltype(&::IMC100_MovJ_RobPos);
using FnMovLRobPos = decltype(&::IMC100_MovL_RobPos);
using FnMovJAbsRobJPos = decltype(&::IMC100_MovJAbs_RobJPos);
using FnGetRobPosHere = decltype(&::IMC100_Get_RobPosHere);
using FnGetRobJPosHere = decltype(&::IMC100_Get_RobJPosHere);
using FnGetRobJToRobP = decltype(&::IMC100_Get_RobJToRobP);
using FnGetB = decltype(&::IMC100_Get_B);
using FnSetB = decltype(&::IMC100_Set_B);
using FnGetR = decltype(&::IMC100_Get_R);
using FnSetR = decltype(&::IMC100_Set_R);
using FnGetD = decltype(&::IMC100_Get_D);
using FnSetD = decltype(&::IMC100_Set_D);
using FnSetDo = decltype(&::IMC100_Set_DO);
using FnGetTaskPrgPath = decltype(&::IMC100_Get_TaskPrgPath);

struct Imc100Api
{
#ifdef _WIN32
    HMODULE module = nullptr;
#endif
    FnInitEth initEth = nullptr;
    FnExitEth exitEth = nullptr;
    FnCurPermit curPermit = nullptr;
    FnAcqPermit acqPermit = nullptr;
    FnRemovePermit removePermit = nullptr;
    FnGetEStopSts getEStopSts = nullptr;
    FnGetSysErr getSysErr = nullptr;
    FnGetMotorSts getMotorSts = nullptr;
    FnGetMotionSts getMotionSts = nullptr;
    FnGetRobotType getRobotType = nullptr;
    FnGetRobotAxisNum getRobotAxisNum = nullptr;
    FnMotorEnable motorEnable = nullptr;
    FnResetErr resetErr = nullptr;
    FnSetMode setMode = nullptr;
    FnPrgCtrl prgCtrl = nullptr;
    FnBackStartLine backStartLine = nullptr;
    FnSetVel setVel = nullptr;
    FnSetAcc setAcc = nullptr;
    FnSetAccRamp setAccRamp = nullptr;
    FnDsMode dsMode = nullptr;
    FnSetCoordType setCoordType = nullptr;
    FnAxisJog axisJog = nullptr;
    FnHome home = nullptr;
    FnMovJRobPos movJRobPos = nullptr;
    FnMovLRobPos movLRobPos = nullptr;
    FnMovJAbsRobJPos movJAbsRobJPos = nullptr;
    FnGetRobPosHere getRobPosHere = nullptr;
    FnGetRobJPosHere getRobJPosHere = nullptr;
    FnGetRobJToRobP getRobJToRobP = nullptr;
    FnGetB getB = nullptr;
    FnSetB setB = nullptr;
    FnGetR getR = nullptr;
    FnSetR setR = nullptr;
    FnGetD getD = nullptr;
    FnSetD setD = nullptr;
    FnSetDo setDo = nullptr;
    FnGetTaskPrgPath getTaskPrgPath = nullptr;

    bool ensureLoaded()
    {
#ifndef _WIN32
        return false;
#else
        if (module) {
            return true;
        }

        module = LoadLibraryW(L"IMC100API.dll");
        if (!module) {
            const std::array<std::filesystem::path, 2> fallbacks = {
                std::filesystem::current_path() / "IMC100API.dll",
                std::filesystem::current_path() / "robot_sdk" / "x64" / "IMC100API.dll"
            };

            for (const auto& candidate : fallbacks) {
                module = LoadLibraryW(candidate.wstring().c_str());
                if (module) {
                    break;
                }
            }
        }

        if (!module) {
            return false;
        }

#define LOAD_REQUIRED(member, symbol) \
        member = reinterpret_cast<decltype(member)>(GetProcAddress(module, symbol)); \
        if (!member) { \
            FreeLibrary(module); \
            module = nullptr; \
            return false; \
        }

#define LOAD_OPTIONAL(member, symbol) \
        member = reinterpret_cast<decltype(member)>(GetProcAddress(module, symbol))

        LOAD_REQUIRED(initEth, "IMC100_Init_ETH");
        LOAD_REQUIRED(exitEth, "IMC100_Exit_ETH");
        LOAD_REQUIRED(curPermit, "IMC100_CurPermit");
        LOAD_REQUIRED(acqPermit, "IMC100_AcqPermit");
        LOAD_REQUIRED(removePermit, "IMC100_RemovePermit");
        LOAD_REQUIRED(getEStopSts, "IMC100_Get_EStopSts");
        LOAD_REQUIRED(getSysErr, "IMC100_Get_SysErr");
        LOAD_REQUIRED(getMotorSts, "IMC100_Get_MotorSts");
        LOAD_REQUIRED(getMotionSts, "IMC100_Get_MotionSts");
        LOAD_REQUIRED(getRobotType, "IMC100_Get_RobotType");
        LOAD_REQUIRED(motorEnable, "IMC100_MotorEnable");
        LOAD_REQUIRED(resetErr, "IMC100_ResetErr");
        LOAD_REQUIRED(setMode, "IMC100_Set_Mode");
        LOAD_REQUIRED(prgCtrl, "IMC100_PrgCtrl");
        LOAD_REQUIRED(backStartLine, "IMC100_BackStartLine");
        LOAD_REQUIRED(setVel, "IMC100_Set_Vel");
        LOAD_REQUIRED(dsMode, "IMC100_DsMode");
        LOAD_REQUIRED(setCoordType, "IMC100_Set_CoordType");
        LOAD_REQUIRED(axisJog, "IMC100_AxisJog");
        LOAD_REQUIRED(home, "IMC100_Home");
        LOAD_REQUIRED(movJRobPos, "IMC100_MovJ_RobPos");
        LOAD_REQUIRED(movLRobPos, "IMC100_MovL_RobPos");
        LOAD_REQUIRED(movJAbsRobJPos, "IMC100_MovJAbs_RobJPos");
        LOAD_REQUIRED(getRobPosHere, "IMC100_Get_RobPosHere");
        LOAD_REQUIRED(getRobJPosHere, "IMC100_Get_RobJPosHere");
        LOAD_REQUIRED(getB, "IMC100_Get_B");
        LOAD_REQUIRED(setB, "IMC100_Set_B");
        LOAD_REQUIRED(getR, "IMC100_Get_R");
        LOAD_REQUIRED(setR, "IMC100_Set_R");
        LOAD_REQUIRED(getD, "IMC100_Get_D");
        LOAD_REQUIRED(setD, "IMC100_Set_D");
        LOAD_REQUIRED(setDo, "IMC100_Set_DO");
        LOAD_REQUIRED(getTaskPrgPath, "IMC100_Get_TaskPrgPath");

        LOAD_OPTIONAL(setAcc, "IMC100_Set_Acc");
        LOAD_OPTIONAL(setAccRamp, "IMC100_Set_AccRamp");
        LOAD_OPTIONAL(getRobotAxisNum, "IMC100_Get_RobotAxisNum");
        LOAD_OPTIONAL(getRobJToRobP, "IMC100_Get_RobJToRobP");

#undef LOAD_OPTIONAL
#undef LOAD_REQUIRED
        return true;
#endif
    }
};

struct SessionState
{
    bool connected = false;
    SOCKETFD fd = -1;
    int currentCoord = 0;
};

Imc100Api& api()
{
    static Imc100Api instance;
    return instance;
}

SessionState& session()
{
    static SessionState instance;
    return instance;
}

bool isConnected(SOCKETFD socketFd)
{
    return session().connected && session().fd == socketFd && socketFd >= 0;
}

Result mapCallResult(int ret)
{
    return ret >= 0 ? SUCCESS : EXCEPTION;
}

int clampPercent(int value)
{
    return std::clamp(value, 1, 100);
}

double clampPercent(double value)
{
    return std::clamp(value, 1.0, 100.0);
}

int mapCoordType(int coord)
{
    switch (coord) {
    case 0:
        return 1;
    case 1:
        return 2;
    case 2:
        return 3;
    case 3:
        return 4;
    default:
        return 2;
    }
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string trimTrailingNulls(const char* text)
{
    if (!text) {
        return {};
    }
    size_t length = 0;
    while (length < 128 && text[length] != '\0') {
        ++length;
    }
    return std::string(text, length);
}

std::string normalizedStem(const std::string& path)
{
    if (path.empty()) {
        return {};
    }

    try {
        return std::filesystem::path(path).stem().string();
    } catch (...) {
        return path;
    }
}

std::string normalizedProjectName(const std::string& path)
{
    if (path.empty()) {
        return {};
    }

    try {
        const std::filesystem::path fsPath(path);
        const std::filesystem::path parent = fsPath.parent_path();
        if (!parent.empty()) {
            const std::string projectName = parent.filename().string();
            if (!projectName.empty()) {
                return projectName;
            }
        }
        return fsPath.stem().string();
    } catch (...) {
        return normalizedStem(path);
    }
}

bool parseIpv4(const std::string& ip, unsigned int* valueOut)
{
    if (!valueOut) {
        return false;
    }

    unsigned int parts[4] = {};
    char dot1 = 0;
    char dot2 = 0;
    char dot3 = 0;
    std::istringstream stream(ip);
    if (!(stream >> parts[0] >> dot1 >> parts[1] >> dot2 >> parts[2] >> dot3 >> parts[3])) {
        return false;
    }
    if (!stream.eof() || dot1 != '.' || dot2 != '.' || dot3 != '.') {
        return false;
    }
    for (unsigned int part : parts) {
        if (part > 255) {
            return false;
        }
    }

    *valueOut = (parts[0] << 24U) | (parts[1] << 16U) | (parts[2] << 8U) | parts[3];
    return true;
}

bool parsePort(const std::string& port, unsigned short* valueOut)
{
    if (!valueOut || port.empty()) {
        return false;
    }

    try {
        const int parsed = std::stoi(port);
        if (parsed <= 0 || parsed > 65535) {
            return false;
        }
        *valueOut = static_cast<unsigned short>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

Result ensureSdkLoaded()
{
    return api().ensureLoaded() ? SUCCESS : DISCONNECT;
}

Result ensureControlPermit(SOCKETFD socketFd)
{
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }

    int owner = 0;
    unsigned int ipAddr = 0;
    unsigned short ipPort = 0;
    if (api().curPermit(&owner, &ipAddr, &ipPort, socketFd) < 0) {
        return EXCEPTION;
    }

    if (owner == 1) {
        return SUCCESS;
    }
    if (owner == 0) {
        if (api().acqPermit(0, socketFd) >= 0) {
            return SUCCESS;
        }
        return api().acqPermit(1, socketFd) >= 0 ? SUCCESS : OPERATION_NOT_ALLOWED;
    }

    // The IMC100 manual recommends forcing permit acquisition when another
    // ethernet client already owns control, otherwise motion/jog commands
    // will fail even though monitoring/connection succeeds.
    return api().acqPermit(1, socketFd) >= 0 ? SUCCESS : OPERATION_NOT_ALLOWED;
}

Result ensureEStopReleased(SOCKETFD socketFd)
{
    int status = 0;
    if (api().getEStopSts(&status, socketFd) < 0) {
        return EXCEPTION;
    }
    return status == 0 ? SUCCESS : OPERATION_NOT_ALLOWED;
}

Result ensureNoSystemError(SOCKETFD socketFd)
{
    int error = 0;
    if (api().getSysErr(&error, socketFd) < 0) {
        return EXCEPTION;
    }
    return error == 0 ? SUCCESS : EXCEPTION;
}

Result ensureMotorEnabled(SOCKETFD socketFd)
{
    int status = 0;
    if (api().getMotorSts(&status, socketFd) < 0) {
        return EXCEPTION;
    }
    return status == 1 ? SUCCESS : OPERATION_NOT_ALLOWED;
}

Result setDataStreamMode(SOCKETFD socketFd, int mode)
{
    return mapCallResult(api().dsMode(mode, socketFd));
}

Result setCoordType(SOCKETFD socketFd, int coord)
{
    const Result result = mapCallResult(api().setCoordType(mapCoordType(coord), socketFd));
    if (result == SUCCESS) {
        session().currentCoord = coord;
    }
    return result;
}

void applyMotionParameters(SOCKETFD socketFd, const MoveCmd& cmd)
{
    api().setVel(clampPercent(cmd.velocity), socketFd);
    if (api().setAcc) {
        api().setAcc(clampPercent(cmd.acc), clampPercent(cmd.dec), socketFd);
    } else if (api().setAccRamp) {
        api().setAccRamp(clampPercent(cmd.acc), clampPercent(cmd.dec), socketFd);
    }
}

void fillVelocity(const MoveCmd& cmd, VER_DATA* velOut)
{
    if (!velOut) {
        return;
    }

    std::memset(velOut, 0, sizeof(*velOut));
    velOut->velType = 0;
    velOut->velPercent = clampPercent(cmd.velocity);
}

void fillRobotPose(const MoveCmd& cmd, ROB_POS* poseOut)
{
    if (!poseOut) {
        return;
    }

    std::memset(poseOut, 0, sizeof(*poseOut));
    for (size_t i = 0; i < 6; ++i) {
        poseOut->RPosData[i] = cmd.targetPosValue[i];
    }
}

void fillJointPose(const MoveCmd& cmd, ROB_JPOS* poseOut)
{
    if (!poseOut) {
        return;
    }

    std::memset(poseOut, 0, sizeof(*poseOut));
    for (size_t i = 0; i < 6; ++i) {
        poseOut->JointData[i] = cmd.targetPosValue[i];
    }
}

Result convertJointToRobotPose(SOCKETFD socketFd, const MoveCmd& cmd, ROB_POS* poseOut)
{
    if (!poseOut) {
        return PARAM_ERR;
    }
    if (!api().getRobJToRobP) {
        return OPERATION_NOT_ALLOWED;
    }

    ROB_JPOS jointPose{};
    fillJointPose(cmd, &jointPose);
    return mapCallResult(api().getRobJToRobP(&jointPose, 0, 0, 0, poseOut, socketFd));
}

Result prepareMotionControl(SOCKETFD socketFd)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureEStopReleased(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureNoSystemError(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureMotorEnabled(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return setDataStreamMode(socketFd, 1);
}

Result prepareJogControl(SOCKETFD socketFd)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureEStopReleased(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureNoSystemError(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureMotorEnabled(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = mapCallResult(api().setMode(1, socketFd));
    if (result != SUCCESS) {
        return result;
    }
    result = setCoordType(socketFd, session().currentCoord);
    if (result != SUCCESS) {
        return result;
    }
    return setDataStreamMode(socketFd, 0);
}

Result prepareProgramControl(SOCKETFD socketFd)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureEStopReleased(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureNoSystemError(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    result = ensureMotorEnabled(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().setMode(2, socketFd));
}

bool parseVariant(const std::string& name, char* kindOut, int* indexOut)
{
    if (!kindOut || !indexOut) {
        return false;
    }

    const std::string upper = toLower(name);
    std::string prefix;
    std::string suffix;

    if (upper.rfind("gb", 0) == 0) {
        prefix = "b";
        suffix = name.substr(2);
    } else if (upper.rfind("gi", 0) == 0) {
        prefix = "r";
        suffix = name.substr(2);
    } else if (upper.rfind("gd", 0) == 0) {
        prefix = "d";
        suffix = name.substr(2);
    } else if (!upper.empty() && (upper[0] == 'b' || upper[0] == 'r' || upper[0] == 'd')) {
        prefix = upper.substr(0, 1);
        suffix = name.substr(1);
    } else {
        return false;
    }

    if (suffix.empty() || !std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return false;
    }

    try {
        const int index = std::stoi(suffix);
        if (index < 0 || index > 255) {
            return false;
        }
        *kindOut = static_cast<char>(std::toupper(static_cast<unsigned char>(prefix[0])));
        *indexOut = index;
        return true;
    } catch (...) {
        return false;
    }
}

int inferLegacyRobotType(const std::string& robotTypeText)
{
    const std::string lower = toLower(robotTypeText);
    int axisCount = 0;
    if (api().getRobotAxisNum) {
        api().getRobotAxisNum(&axisCount, kDefaultComId);
    }

    if (lower.find("scara") != std::string::npos) {
        if (axisCount == 2) return 8;
        if (axisCount == 3) return 9;
        return 2;
    }

    switch (axisCount) {
    case 1:
        return 5;
    case 4:
        return 4;
    case 5:
        return 6;
    case 6:
        return 1;
    case 7:
        return 12;
    default:
        return 12;
    }
}

std::string readCurrentTaskPath(SOCKETFD socketFd)
{
    char pathBuffer[128] = {};
    if (api().getTaskPrgPath(kMainTaskId, pathBuffer, socketFd) < 0) {
        return {};
    }
    return trimTrailingNulls(pathBuffer);
}

} // namespace

SOCKETFD connect_robot(const std::string& ip, const std::string& port)
{
    if (ensureSdkLoaded() != SUCCESS) {
        return -1;
    }

    if (session().connected && session().fd >= 0) {
        disconnect_robot(session().fd);
    }

    unsigned int ipValue = 0;
    unsigned short portValue = 0;
    if (!parseIpv4(ip, &ipValue) || !parsePort(port, &portValue)) {
        return -1;
    }

    if (api().initEth(ipValue, portValue, kSdkTimeoutSeconds, kDefaultComId) < 0) {
        return -1;
    }

    session().connected = true;
    session().fd = kDefaultComId;
    session().currentCoord = 0;
    return kDefaultComId;
}

Result disconnect_robot(SOCKETFD socketFd)
{
    if (ensureSdkLoaded() != SUCCESS) {
        return DISCONNECT;
    }
    if (!isConnected(socketFd)) {
        session() = {};
        return SUCCESS;
    }

    api().removePermit(socketFd);
    api().dsMode(0, socketFd);
    const Result result = mapCallResult(api().exitEth(socketFd));
    session() = {};
    return result;
}

Result set_servo_state(SOCKETFD socketFd, int state)
{
    const Result result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().motorEnable(state != 0 ? 1 : 0, socketFd));
}

Result set_servo_poweron(SOCKETFD socketFd)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().motorEnable(1, socketFd));
}

Result set_servo_poweroff(SOCKETFD socketFd)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    api().dsMode(0, socketFd);
    return mapCallResult(api().motorEnable(0, socketFd));
}

Result clear_error(SOCKETFD socketFd)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().resetErr(socketFd));
}

Result set_current_mode(SOCKETFD socketFd, int mode)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }

    const int sdkMode = (mode == 0) ? 1 : 2;
    return mapCallResult(api().setMode(sdkMode, socketFd));
}

Result set_current_coord(SOCKETFD socketFd, int coord)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    return setCoordType(socketFd, coord);
}

Result set_speed(SOCKETFD socketFd, int speed)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().setVel(clampPercent(speed), socketFd));
}

Result robot_start_jogging(SOCKETFD socketFd, int axis, bool positiveDirection)
{
    Result result = prepareJogControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().axisJog(axis, positiveDirection ? 1 : -1, socketFd));
}

Result robot_stop_jogging(SOCKETFD socketFd, int axis)
{
    Result result = prepareJogControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().axisJog(axis, 0, socketFd));
}

Result robot_movej(SOCKETFD socketFd, const MoveCmd& cmd)
{
    Result result = prepareMotionControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }

    applyMotionParameters(socketFd, cmd);
    if (cmd.coord == 0) {
        ROB_JPOS jointPose{};
        VER_DATA velocity{};
        fillJointPose(cmd, &jointPose);
        fillVelocity(cmd, &velocity);
        return mapCallResult(api().movJAbsRobJPos(&jointPose, &velocity, 0, 0, nullptr, socketFd));
    }

    result = setCoordType(socketFd, cmd.coord);
    if (result != SUCCESS) {
        return result;
    }

    ROB_POS robotPose{};
    VER_DATA velocity{};
    fillRobotPose(cmd, &robotPose);
    fillVelocity(cmd, &velocity);
    return mapCallResult(api().movJRobPos(&robotPose, &velocity, 0, 0, nullptr, socketFd));
}

Result robot_movel(SOCKETFD socketFd, const MoveCmd& cmd)
{
    Result result = prepareMotionControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }

    applyMotionParameters(socketFd, cmd);

    ROB_POS robotPose{};
    if (cmd.coord == 0) {
        result = convertJointToRobotPose(socketFd, cmd, &robotPose);
        if (result != SUCCESS) {
            return result;
        }
    } else {
        result = setCoordType(socketFd, cmd.coord);
        if (result != SUCCESS) {
            return result;
        }
        fillRobotPose(cmd, &robotPose);
    }

    VER_DATA velocity{};
    fillVelocity(cmd, &velocity);
    return mapCallResult(api().movLRobPos(&robotPose, &velocity, 0, 0, nullptr, socketFd));
}

Result robot_go_home(SOCKETFD socketFd)
{
    Result result = prepareMotionControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().home(kHomeIndex, socketFd));
}

Result robot_go_to_reset_position(SOCKETFD socketFd)
{
    return robot_go_home(socketFd);
}

Result get_current_position(SOCKETFD socketFd, int positionType, std::vector<double>& poseOut)
{
    poseOut.clear();

    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }

    if (positionType == 0) {
        ROB_JPOS pose{};
        if (api().getRobJPosHere(&pose, socketFd) < 0) {
            return EXCEPTION;
        }
        poseOut.assign(pose.JointData, pose.JointData + 6);
        return SUCCESS;
    }

    ROB_POS pose{};
    if (api().getRobPosHere(&pose, socketFd) < 0) {
        return EXCEPTION;
    }
    poseOut.assign(pose.RPosData, pose.RPosData + 6);
    return SUCCESS;
}

Result get_servo_state(SOCKETFD socketFd, int& stateOut)
{
    stateOut = 0;

    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }

    int error = 0;
    if (api().getSysErr(&error, socketFd) >= 0 && error != 0) {
        stateOut = 2;
        return SUCCESS;
    }

    int motionState = 0;
    api().getMotionSts(&motionState, socketFd);
    if (motionState == 1) {
        stateOut = 3;
        return SUCCESS;
    }

    int motorState = 0;
    if (api().getMotorSts(&motorState, socketFd) < 0) {
        return EXCEPTION;
    }
    stateOut = motorState == 1 ? 1 : 0;
    return SUCCESS;
}

Result get_robot_running_state(SOCKETFD socketFd, int& stateOut)
{
    stateOut = 0;

    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }

    int motionState = 0;
    if (api().getMotionSts(&motionState, socketFd) < 0) {
        return EXCEPTION;
    }

    if (motionState == 1) {
        stateOut = 2;
    } else if (motionState == 2) {
        stateOut = 1;
    } else {
        stateOut = 0;
    }
    return SUCCESS;
}

Result get_robot_type(SOCKETFD socketFd, int& typeOut)
{
    typeOut = 12;

    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }

    char typeBuffer[128] = {};
    if (api().getRobotType(typeBuffer, socketFd) < 0) {
        return EXCEPTION;
    }

    typeOut = inferLegacyRobotType(trimTrailingNulls(typeBuffer));
    return SUCCESS;
}

Result set_global_variant(SOCKETFD socketFd, const std::string& name, double value)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }

    char kind = '\0';
    int index = 0;
    if (!parseVariant(name, &kind, &index)) {
        return PARAM_ERR;
    }

    switch (kind) {
    case 'B':
        return mapCallResult(api().setB(index, value >= 0.5 ? 1 : 0, socketFd));
    case 'R':
        return mapCallResult(api().setR(index, static_cast<int>(value), socketFd));
    case 'D':
        return mapCallResult(api().setD(index, value, socketFd));
    default:
        return PARAM_ERR;
    }
}

Result get_global_variant(SOCKETFD socketFd, const std::string& name, double& valueOut)
{
    valueOut = 0.0;

    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }

    char kind = '\0';
    int index = 0;
    if (!parseVariant(name, &kind, &index)) {
        return PARAM_ERR;
    }

    switch (kind) {
    case 'B': {
        int value = 0;
        result = mapCallResult(api().getB(index, &value, socketFd));
        valueOut = static_cast<double>(value);
        return result;
    }
    case 'R': {
        int value = 0;
        result = mapCallResult(api().getR(index, &value, socketFd));
        valueOut = static_cast<double>(value);
        return result;
    }
    case 'D':
        return mapCallResult(api().getD(index, &valueOut, socketFd));
    default:
        return PARAM_ERR;
    }
}

Result set_digital_output(SOCKETFD socketFd, int port, int value)
{
    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    result = ensureControlPermit(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    if (port < 1 || port > 255) {
        return PARAM_ERR;
    }
    return mapCallResult(api().setDo(port, value != 0 ? 1 : 0, socketFd));
}

Result job_run_times(SOCKETFD socketFd, int times)
{
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }
    if (times == 1) {
        return SUCCESS;
    }
    return OPERATION_NOT_ALLOWED;
}

Result job_get_current_task_program_path(SOCKETFD socketFd, std::string& pathOut)
{
    pathOut.clear();

    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return result;
    }
    if (!isConnected(socketFd)) {
        return DISCONNECT;
    }

    pathOut = readCurrentTaskPath(socketFd);
    return pathOut.empty() ? EXCEPTION : SUCCESS;
}

Result job_diagnose_start_conditions(SOCKETFD socketFd, std::vector<JobStartCheckItem>& itemsOut)
{
    itemsOut.clear();

    auto addItem = [&itemsOut](const std::string& name, Result result, const std::string& detail) {
        JobStartCheckItem item;
        item.name = name;
        item.result = result;
        item.detail = detail;
        itemsOut.push_back(item);
        return result;
    };

    Result result = ensureSdkLoaded();
    if (result != SUCCESS) {
        return addItem("SDK loaded", result, "IMC100API.dll load failed");
    }
    addItem("SDK loaded", SUCCESS, "ok");

    if (!isConnected(socketFd)) {
        return addItem("Connection", DISCONNECT, "socket is not connected");
    }
    addItem("Connection", SUCCESS, "socket=" + std::to_string(socketFd));

    int owner = 0;
    unsigned int permitIp = 0;
    unsigned short permitPort = 0;
    if (api().curPermit(&owner, &permitIp, &permitPort, socketFd) < 0) {
        return addItem("Control permit", EXCEPTION, "IMC100_CurPermit failed");
    }

    Result permitResult = SUCCESS;
    std::ostringstream permitDetail;
    permitDetail << "owner=" << owner << ", ip=0x" << std::hex << permitIp
                 << std::dec << ", port=" << permitPort;
    if (owner != 1) {
        const int acqNormal = api().acqPermit(0, socketFd);
        const int acqForce = acqNormal >= 0 ? acqNormal : api().acqPermit(1, socketFd);
        permitDetail << ", acqNormal=" << acqNormal << ", acqForce=" << acqForce;
        permitResult = acqForce >= 0 ? SUCCESS : OPERATION_NOT_ALLOWED;
    }
    addItem("Control permit", permitResult, permitDetail.str());
    if (permitResult != SUCCESS) {
        return permitResult;
    }

    int eStopStatus = 0;
    if (api().getEStopSts(&eStopStatus, socketFd) < 0) {
        return addItem("E-stop", EXCEPTION, "IMC100_Get_EStopSts failed");
    }
    result = eStopStatus == 0 ? SUCCESS : OPERATION_NOT_ALLOWED;
    addItem("E-stop", result, "status=" + std::to_string(eStopStatus));
    if (result != SUCCESS) {
        return result;
    }

    int systemError = 0;
    if (api().getSysErr(&systemError, socketFd) < 0) {
        return addItem("System error", EXCEPTION, "IMC100_Get_SysErr failed");
    }
    result = systemError == 0 ? SUCCESS : EXCEPTION;
    addItem("System error", result, "error=" + std::to_string(systemError));
    if (result != SUCCESS) {
        return result;
    }

    int motorStatus = 0;
    if (api().getMotorSts(&motorStatus, socketFd) < 0) {
        return addItem("Motor enabled", EXCEPTION, "IMC100_Get_MotorSts failed");
    }
    result = motorStatus == 1 ? SUCCESS : OPERATION_NOT_ALLOWED;
    addItem("Motor enabled", result, "status=" + std::to_string(motorStatus));
    if (result != SUCCESS) {
        return result;
    }

    result = mapCallResult(api().setMode(2, socketFd));
    addItem("Set auto mode", result, "IMC100_Set_Mode(2)");
    return result;
}

Result job_run(SOCKETFD socketFd, const std::string& name)
{
    Result result = prepareProgramControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }

    const std::string currentPath = readCurrentTaskPath(socketFd);
    if (!name.empty() && !currentPath.empty()) {
        const std::string expected = toLower(name);
        const std::string current = toLower(normalizedProjectName(currentPath));
        if (expected != current) {
            return OPERATION_NOT_ALLOWED;
        }
    }

    return mapCallResult(api().prgCtrl(1, socketFd));
}

Result job_pause(SOCKETFD socketFd)
{
    Result result = prepareProgramControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().prgCtrl(0, socketFd));
}

Result job_continue(SOCKETFD socketFd)
{
    Result result = prepareProgramControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().prgCtrl(1, socketFd));
}

Result job_reset(SOCKETFD socketFd)
{
    Result result = prepareProgramControl(socketFd);
    if (result != SUCCESS) {
        return result;
    }
    return mapCallResult(api().backStartLine(socketFd));
}

Result job_stop(SOCKETFD socketFd)
{
    Result pauseResult = job_pause(socketFd);
    if (pauseResult != SUCCESS) {
        return pauseResult;
    }
    return job_reset(socketFd);
}
