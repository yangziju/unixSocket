#ifndef _DOMAIN_COMMON_
#define _DOMAIN_COMMON_
#include <iostream>

const std::string kServerAddress = "./unix.sock";

// client configure
const int kReConnectCount = 2;
const int kReconnectInterval = 3; // s
const int kCleanTimeoutRequest = 1000; // ms
const int kBufferSize = 5120;

typedef void (*disconnect_event)();
typedef void (*async_result_cb)(char* data, uint64_t size);
typedef std::string (*response_fun)(char* data, uint64_t size);

struct RpcRequestHdr
{
    unsigned long long id;
    int data_size;
    char data[];
};

struct RequestValue
{
    struct timespec time;
    async_result_cb cbk;
};

#endif // _DOMAIN_COMMON_