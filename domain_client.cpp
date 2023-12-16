#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <assert.h>
#include "domain_client.h"

UDSockClient::UDSockClient()
    :buffer_size_(1), sock_(-1), running_(false) 
{

}

UDSockClient::~UDSockClient() 
{
    if(sock_ != -1)
    {
        close(sock_);
    }
}

int UDSockClient::Init(const std::string server_addr, disconnect_event disconnect_fun)
{
    on_disconnect = disconnect_fun;
    addr_.sun_family = AF_UNIX;
    std::strcpy(addr_.sun_path, server_addr.c_str());
    int ret = 0;
    
    if ((sock_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        return -errno;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) == -1) 
    {
        ret = -errno;
        CleanSocket("init");
        return ret;
    }

    if (connect(sock_, (struct sockaddr*)&addr_, sizeof(addr_)) == -1) 
    {
        ret = -errno;
        CleanSocket("init");
        return ret;
    }

    thread_ = std::thread(&UDSockClient::Run, this);

    return 0;
}

int UDSockClient::ConnectServer()
{
    int max_retry = kReConnectCount;
    int ret = 0;
    if ((sock_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        return -errno;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        return -errno;
    }

    for (int retry = 1; retry <= max_retry; retry++)
    {
        if (connect(sock_, (struct sockaddr*)&addr_, sizeof(addr_)) == -1) 
        {
            std::cerr << "connect server failed, retry count:" << retry << std::endl;
            sleep(kReconnectInterval);
            clean_timeout_requeset();
            continue;
        }
        return 0;
    }

    on_disconnect();
    return -ECONNREFUSED;
}

void UDSockClient::Run()
{
    RpcRequestHdr head;
    uint64_t bytes_received = 0;
    running_ = true;
    char* reserve_buff = (char*)malloc(buffer_size_);
    if(!reserve_buff) 
        return;
    char* recv_buff = reserve_buff;
    bool is_free = false;

    while(running_)
    {
        if (is_free)
        {
            free(recv_buff);
            recv_buff = reserve_buff;
            is_free = false;
        }

        clean_timeout_requeset();

        if (sock_ == -1)
        {
            if(ConnectServer() < 0)
            {
                CleanSocket("connect");
                continue;
            }
        }

        bzero(&head, sizeof(head));

        if (RecvBytes((char*)&head, sizeof(RpcRequestHdr)) < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                CleanSocket("recv head");
            continue;
        }

        if (head.data_size == 0) continue;

        if (buffer_size_ < head.data_size)
        {
            if (!(recv_buff = (char*)malloc(head.data_size)))
            {
                std::cerr << "domain socket, malloc failed" << std::endl;
                recv_buff = reserve_buff;
                continue;
            }
            is_free = true;
        }

        // std::cout << "DEBUG recv body size: " << head.data_size << std::endl;
        bzero(recv_buff, head.data_size);
        if (RecvBytes(recv_buff, head.data_size) < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                CleanSocket("recv body");
            continue;
        }

        {
            std::lock_guard<std::mutex> _(lock_req_);
            auto it = request_.find(head.id);
            if (it != request_.end())
            {
                it->second.cbk((char*)recv_buff, head.data_size);
                request_.erase(head.id);
            }
        }
    }
    if (reserve_buff)
    {
        free(reserve_buff);
        reserve_buff = nullptr;
    }
}

int UDSockClient::SendRequest(std::string& request, async_result_cb result_cbk)
{
    static unsigned long long request_id = 0;
    int ret = 0;
    RequestValue value;
    value.cbk = result_cbk;
    clock_gettime(CLOCK_REALTIME, &value.time);

    int buff_size = sizeof(RpcRequestHdr) + request.size();
    char* send_buff = (char*)malloc(sizeof(char) * buff_size);
    if (!send_buff)
    {
        return -errno;
    }

    RpcRequestHdr* head = reinterpret_cast<RpcRequestHdr*>(send_buff);
    head->data_size = request.size();
    head->data = send_buff + sizeof(RpcRequestHdr);
    memcpy(head->data, request.c_str(), request.size());

    {
        std::lock_guard<std::mutex> _(lock_req_);
        head->id = request_id++;
        request_.insert(std::make_pair(head->id, value));
    }

    {
        std::lock_guard<std::mutex> _(lock_send_);
        if (SendBytes(send_buff, buff_size) <= 0)
        {
            ret = -errno;
        }
    }

    if (send_buff)
    {
        free(send_buff);
    }

    return ret;
}

void UDSockClient::Stop()
{
    running_ = false;
    if (thread_.joinable())
    {
        thread_.join();
    }
}


inline int64_t UDSockClient::diff_ms(struct timespec& start, struct timespec& now)
{
    return (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
}

inline void UDSockClient::clean_timeout_requeset()
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    std::lock_guard<std::mutex> _(lock_req_);
    for (auto it = request_.begin(); it != request_.end();) 
    {
        if (diff_ms(it->second.time, now) >= kCleanTimeoutRequest) {
            std::cout << "DEBUG clean request: " << it->first << " left count: " << request_.size() << std::endl;
            it = request_.erase(it);
        } else {
            ++it;
        }
    }
}

bool UDSockClient::IsConnected()
{
    return sock_ != -1;
}

void UDSockClient::CleanSocket(std::string str)
{
    std::cout << "DEBUG " << str << " errno = " << errno << std::endl;
    if (sock_ != -1)
    {
        close(sock_);
        sock_ = -1;
    }
}

int64_t UDSockClient::RecvBytes(char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    n = recv(sock_, (void*)buff, nbytes, 0);
    if (n == -1) {
        if (errno == EINTR)
            goto again;
        else
            return -1;
    } else if (n == 0) {
        return -1;
    }

    buff += n;
    nbytes -= n;
    if (nbytes > 0) {
        std::cout << "DEBUG again recv, left size = " << nbytes << " n = " << n << std::endl;
        goto again;
    }
    assert(nbytes == 0);
    return nbytes;
}

int64_t UDSockClient::SendBytes(const char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    if ((n = send(sock_, (void*)buff, nbytes, MSG_NOSIGNAL)) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            goto again;
        else
            return -1;
    }
    buff += n;
    nbytes -= n;
    if (nbytes > 0) {
        std::cout << "DEBUG again send, left size = " << nbytes << std::endl;
        goto again;
    }
    assert(nbytes == 0);
    return n;
}

#if 1

UDSockClient client;

void disconn_event()
{
    std::cerr << "server quit...!!!" << std::endl;
}

void do_respone(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    uint64_t recv_id = atoll(resp_buff);
    if (recv_id != ++expect_id)
        std::cerr << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    // else std::cout << "1 success" << std::endl;
}

void loop_send()
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(++req_id));
        if(client.SendRequest(req, do_respone) < 0) 
        {
            // perror("send req failed");
        }
        usleep(20000);

    }
}

void do_respone2(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    uint64_t recv_id = atoll(resp_buff);
    if (recv_id != ++expect_id)
        std::cerr << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    // else std::cout << "2 success" << std::endl;
}

void loop_send2()
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(++req_id));
        if(client.SendRequest(req, do_respone2) < 0) 
        {
            // perror("send req failed");
        }
        usleep(20000);
    }
}

void do_respone3(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    uint64_t recv_id = atoll(resp_buff);
    if (recv_id != ++expect_id)
        std::cerr << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    // else std::cout << "3 success" << std::endl;
}

void loop_send3()
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(++req_id));
        if(client.SendRequest(req, do_respone3) < 0) 
        {
            // perror("send req failed");
        }
        usleep(20000);
    }
}

int main() {

    try
    {
        const int th_nums = 3;
        if (client.Init(kServerAddress, &disconn_event) < 0)
        {
            perror("Init");
            return 1;
        }
        std::thread threads[th_nums];
        threads[0] = std::thread(&loop_send);
        threads[1] = std::thread(&loop_send2);
        threads[2] = std::thread(&loop_send3);

        for (int i = 0; i < th_nums; i++)
        {
            threads[i].join();
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}
#endif