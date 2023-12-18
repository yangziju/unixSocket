#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <map>
#include <functional>
#include "domain_common.h"

class UDSockClient
{
    using disconnect_event = std::function<void()>;
    using async_result_cb = std::function<void(char* data, uint64_t size)>;

    struct RequestValue
    {
        struct timespec time;
        async_result_cb cbk;
    };
public:
    UDSockClient();

    ~UDSockClient();

    int Init(const std::string server_addr, const disconnect_event& disconnect_fun);

    void Run();

    int SendRequest(std::string& request, const async_result_cb& result_cbk);

    void Stop();

    bool IsConnected();

protected:
    inline int64_t diff_ms(struct timespec& start, struct timespec& now);

    inline void clean_timeout_requeset();

    void CleanSocket(std::string str);

    int ConnectServer();

    int64_t RecvBytes(char* buff, int64_t nbytes);

    int64_t SendBytes(const char* buff, int64_t nbytes);

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
