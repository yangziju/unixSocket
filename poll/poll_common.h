#ifndef _DOMAIN_COMMON_
#define _DOMAIN_COMMON_
#include <iostream>
#include <fcntl.h>

const int kBufferSize = 5120;
const std::string kServerAddress = "/tmp/unix.sock";

// server configure
const int kMaxFiles = 1024;

// client configure
const int kReConnectCount = 2;
const int kReconnectInterval = 3; // s
const uint64_t kCleanTimeoutRequest = 3000; // ms

struct RpcRequestHdr
{
    uint64_t id;
    uint32_t data_size;
    char data[];
};

#define CLOSE_FD(fd) \
    do  \
    {   \
        if ((fd) != -1) \
        { \
            close((fd));    \
            (fd) = -1;      \
        } \
    } while (0); \


inline void LOG_OUT(const std::string& info, const std::string& str)
{
    std::cout << info << " " << str << std::endl;
}

class UDSockBase
{
protected:
    int SetNonBlocking(int sockfd) 
    {
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) 
        {
            return -errno;
        }

        flags |= O_NONBLOCK;
        
        if (fcntl(sockfd, F_SETFL, flags) == -1) 
        {
            return -errno;
        }

        return 0;
    }
};



#endif // _DOMAIN_COMMON_