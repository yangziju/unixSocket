#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <map>
#include <functional>
#include "poll_common.h"

class UDSockClient : protected UDSockBase
{
    using disconnect_event = std::function<void()>;
    using ResponseCbk = std::function<void(char* data, uint64_t size)>;

    struct RequestValue
    {
        struct timespec time;
        ResponseCbk cbk;
    };

public:
    UDSockClient();

    ~UDSockClient();

    int Init(const std::string server_addr, const disconnect_event& disconnect_fun);

    void Run();

    int SendRequest(std::string& request, const ResponseCbk& result_cbk);

    void Stop();

    bool IsConnected();

protected:

    void CleanTimeoutRequest();

    void CleanSocket(std::string str);

    int ConnectServer();

private:

    uint32_t buffer_size_;
    int sock_;
    bool running_;
    sockaddr_un addr_;
    disconnect_event on_disconnect;
    std::mutex lock_send_;
    std::thread thread_;

    std::mutex lock_req_;
    std::map<unsigned long long, RequestValue> request_;
};
