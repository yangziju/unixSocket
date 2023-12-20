#include "domain_server.h"
class loop {
public:
    std::string do_sponse(char* data, uint64_t size)
    {
        return std::string(data, size);
    }
    void run() {
        UDSockServer server;
        if (server.Init(kServerAddress, std::bind(&loop::do_sponse, this, std::placeholders::_1, std::placeholders::_2)) < 0)
        {
            perror("init");
        }
        server.Run();
    } 
};

int main()
{
    loop l;
    l.run();
    return 0;
}