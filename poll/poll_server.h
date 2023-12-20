#include <thread>
#include <functional>
#include "poll_common.h"

class UDSockServer 
{
using ResultCbk = std::function<std::string(char* data, uint64_t size)>;
public:
    UDSockServer();

    ~UDSockServer();

    bool Init(const std::string& server_addr, const ResultCbk& on_response);

    int Run();

    void Stop();

protected:

    int64_t RecvBytes(int fd, char* buff, int64_t nbytes);

    int64_t SendBytes(int fd, const char* buff, int64_t nbytes);

private:

    int lis_sock_;
    uint32_t buffer_size_;
    std::thread thread_;
    bool running_;
    std::string address_;
    ResultCbk on_response_;
};
