#ifndef ROBOT_SDK_CPP_ROBOT_ADAPTER_H
#define ROBOT_SDK_CPP_ROBOT_ADAPTER_H

#include <string>

enum class RobotBrand {
    Huichuan = 0,
    Yinglian = 1
};

void set_robot_brand(RobotBrand brand);
RobotBrand robot_brand();
const char* robot_brand_name(RobotBrand brand);
RobotBrand robot_brand_from_name(const std::string& name);

#endif // ROBOT_SDK_CPP_ROBOT_ADAPTER_H
