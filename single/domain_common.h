#ifndef _DOMAIN_COMMON_
#define _DOMAIN_COMMON_
#include <iostream>

const std::string kServerAddress = "/tmp/unix.sock";

// client configure
const int kReConnectCount = 2;
const int kReconnectInterval = 3; // s
const int kCleanTimeoutRequest = 3000; // ms
const int kBufferSize = 5120;

struct RpcRequestHdr
{
    unsigned long long id;
    uint32_t data_size;
    char data[];
};


#endif // _DOMAIN_COMMON_