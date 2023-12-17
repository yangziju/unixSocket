#include "domain_server.h"
#include <unistd.h>

std::string do_sponse(char* data, uint64_t size)
{
    return std::string(data, size);
}

int main()
{
    UDSockServer server;
    if (server.Init(kServerAddress, do_sponse) < 0)
    {
        perror("init");
    }
    sleep(10);
    server.Stop();
    return 0;
}
