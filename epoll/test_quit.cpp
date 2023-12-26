#include "poll_server.h"
#include <unistd.h>
class loop {
public:
    std::string do_sponse(char* data, uint64_t size)
    {
        return std::string(data, size);
    }
    void run() {
        UDSockServer server;
        if (!server.Init(kServerAddress, std::bind(&loop::do_sponse, this, std::placeholders::_1, std::placeholders::_2)))
        {
            perror("init");
        }
        sleep(3);
        server.Stop();
    } 
};

int main()
{
    signal(SIGPIPE, SIG_IGN);
    loop l;
    l.run();
    return 0;
}