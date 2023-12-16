#include <thread>
#include "domain_common.h"

class UDSockServer 
{
public:
    UDSockServer();

    ~UDSockServer();

    int Init(const std::string server_addr, response_fun on_response);

    int Run();

    void stop();

protected:

    void CleanSocket();

    uint64_t RecvBytes(void* buff, size_t nbytes);

    uint64_t SendBytes(const void* buff, size_t nbytes);

private:

    int lis_sock_;
    int cli_sock_;
    uint32_t buffer_size_;
    std::thread thread_;
    bool running_;
    std::string address_;
    response_fun on_response_;
};
