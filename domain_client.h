#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <map>
#include "domain_common.h"

class UDSockClient
{
public:
    UDSockClient();

    ~UDSockClient();

    int Init(const std::string server_addr, disconnect_event disconnect_fun);

    void Run();

    int SendRequest(std::string& request, async_result_cb result_cbk);

    void Stop();

protected:
    inline int64_t diff_ms(struct timespec& start, struct timespec& now);

    inline void clean_timeout_requeset();

    void Disconnect(std::string str);

    int64_t RecvBytes(char* buff, int64_t nbytes);

    int64_t SendBytes(const char* buff, int64_t nbytes);

private:

    int buffer_size_;
    int sock_;
    bool is_connected_;
    bool running_;
    sockaddr_un addr_;
    disconnect_event on_disconnect;
    std::thread thread_;

    std::mutex lock_;
    std::map<unsigned long long, RequestValue> request_;
};
