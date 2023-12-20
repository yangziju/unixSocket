#include <thread>
#include <functional>
#include "poll_common.h"

class UDSockServer : protected UDSockBase
{
using RequestCbk = std::function<std::string(char* data, uint64_t size)>;

public:
    UDSockServer();

    ~UDSockServer();

    bool Init(const std::string& server_addr, const RequestCbk& on_request);

    int Run();

    void Stop();

protected:

    bool Accept(struct pollfd* fds, int& maxi, Buffer* buffs);

private:

    int lis_sock_;
    uint32_t buffer_size_;
    std::thread thread_;
    bool running_;
    std::string address_;
    RequestCbk on_request_;
};
