#include <thread>
#include <functional>
#include "domain_common.h"

class UDSockServer 
{
using response_fun = std::function<std::string(char* data, uint64_t size)>;
public:
    UDSockServer();

    ~UDSockServer();

    int Init(const std::string server_addr, const response_fun& on_response);

    int Run();

    void Stop();

protected:

    void CleanSocket();

    int64_t RecvBytes(char* buff, int64_t nbytes);

    int64_t SendBytes(const char* buff, int64_t nbytes);

private:

    int lis_sock_;
    int cli_sock_;
    uint32_t buffer_size_;
    std::thread thread_;
    bool running_;
    std::string address_;
    response_fun on_response_;
};
