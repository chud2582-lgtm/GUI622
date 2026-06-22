#ifndef NRC_INTERFACE_PARAMETER_H
#define NRC_INTERFACE_PARAMETER_H

#include <array>

enum class PosType {
    data = 0
};

struct MoveCmd
{
    PosType targetPosType = PosType::data;
    int coord = 0;
    int velocity = 0;
    double acc = 0.0;
    double dec = 0.0;
    std::array<double, 7> targetPosValue = {};
};

#endif // NRC_INTERFACE_PARAMETER_H
