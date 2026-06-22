#ifndef NRC_IO_H
#define NRC_IO_H

#include "../parameter/nrc_define.h"

Result set_digital_output(SOCKETFD socketFd, int port, int value);
Result get_digital_input(SOCKETFD socketFd, int port, int& value);
Result get_digital_output(SOCKETFD socketFd, int port, int& value);

#endif // NRC_IO_H
