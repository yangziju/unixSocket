#include <thread>
#include <functional>
#include <unordered_map>
#include "poll_common.h"

class UDSockServer : protected SockIO
{
using RequestCbk = std::function<std::string(char* data, uint64_t size)>;

public:
    UDSockServer(const int& buffer_size = 5120);

    ~UDSockServer();

    bool Init(const std::string& server_addr, const RequestCbk& on_request);

    int Run();

    void Stop();

protected:

    bool Accept(int efd, int fd, std::unordered_map<int, Buffer*>& conn);

private:

    int lis_sock_;
    uint32_t buffer_size_;
    std::thread thread_;
    std::string address_;
    RequestCbk on_request_;
    volatile bool running_;
};
