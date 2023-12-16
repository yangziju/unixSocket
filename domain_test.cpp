#include "domain_client.h"
#include "domain_server.h"

std::string do_sponse(char* data, uint64_t size)
{
    return std::string(data, size);
}

int test_server()
{
    UDSockServer server;
    if (server.Init(kServerAddress, do_sponse) < 0)
    {
        perror("init");
        return -1;
    }
    server.Run();
    return 0;
}

void test_client()
{

}

int main()
{
    if(test_server() < 0) return 0;

    return 0;
}