#include "domain_server.h"
#include <unistd.h>

std::string do_sponse(char* data, uint64_t size)
{
    return std::string(data, size);
}

int main()
{
    UDSockServer server;
    if (server.Init(kServerAddress, std::bind(&do_sponse, std::placeholders::_1, std::placeholders::_2)) < 0)
    {
        perror("init");
    }
    sleep(10);
    server.Stop();
    return 0;
}
