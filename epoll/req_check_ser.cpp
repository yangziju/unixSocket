#include "poll_server.h"
#include <unistd.h>

std::string do_sponse(char* data, uint64_t size)
{
    std::string num_str(data, size);
    int64_t num = std::stoll(num_str);
    std::cout << "recv " << num << std::endl;
    return std::to_string(num * 10);
}

int main()
{
    signal(SIGPIPE, SIG_IGN);
    UDSockServer server(sizeof(RpcRequestHdr) + 1);

    if (!server.Init(kServerAddress, std::bind(&do_sponse, std::placeholders::_1, std::placeholders::_2)))
    {
        perror("init");
        return 1;
    }
    server.Run();
    return 0;
}