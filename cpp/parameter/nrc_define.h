#ifndef NRC_DEFINE_H
#define NRC_DEFINE_H

using SOCKETFD = int;

enum Result {
    SUCCESS = 0,
    RECEIVE_FAILED = -1,
    DISCONNECT = -2,
    PARAM_ERR = -3,
    OPERATION_NOT_ALLOWED = -4,
    EXCEPTION = -5,
    TIMEOUT = -6
};

#endif // NRC_DEFINE_H
