#include "poll_server.h"
#include <unistd.h>

std::string do_sponse(char* data, uint64_t size)
{
    return std::string(data, size);
}

int main()
{
    signal(SIGPIPE, SIG_IGN);
    UDSockServer server;
    if (!server.Init(kServerAddress, std::bind(&do_sponse, std::placeholders::_1, std::placeholders::_2)))
    {
        perror("init");
        return 1;
    }
    server.Run();
    return 0;
}