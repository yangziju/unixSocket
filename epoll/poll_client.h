#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <map>
#include <functional>
#include "poll_common.h"

class UDSockClient : protected SockIO
{
    using OnDisconnct = std::function<void()>;
    using ResponseCbk = std::function<void(char* data, uint64_t size)>;

public:
    UDSockClient(const int& buffer_size = 5120);

    ~UDSockClient();

    bool Init(const std::string& server_addr, const OnDisconnct& on_disconn);

    void Run();

    int SendRequest(std::string& request, const ResponseCbk& result_cbk);

    void Stop();

    bool IsConnected();

protected:

    inline void CleanRequest();

    bool ConnectServer();

private:

    uint32_t buffer_size_;
    int sock_;
    sockaddr_un addr_;
    OnDisconnct on_disconn_;
    std::mutex lock_send_;
    std::thread thread_;
    volatile bool running_;

    std::mutex lock_req_;
    std::map<uint64_t, ResponseCbk> request_;
};
