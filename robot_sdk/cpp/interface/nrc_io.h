#ifndef ROBOT_SDK_CPP_INTERFACE_NRC_IO_H
#define ROBOT_SDK_CPP_INTERFACE_NRC_IO_H

#include "../parameter/nrc_define.h"

Result robot_set_digital_output(SOCKETFD socketFd, int port, int value);
Result robot_get_digital_input(SOCKETFD socketFd, int port, int& value);
Result robot_get_digital_output(SOCKETFD socketFd, int port, int& value);

#endif // ROBOT_SDK_CPP_INTERFACE_NRC_IO_H
