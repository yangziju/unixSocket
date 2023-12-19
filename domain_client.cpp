#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <assert.h>
#include "domain_client.h"

UDSockClient::UDSockClient()
    :buffer_size_(kBufferSize), sock_(-1), running_(false) 
{

}

UDSockClient::~UDSockClient() 
{
    if(sock_ != -1)
    {
        close(sock_);
    }
}

int UDSockClient::Init(const std::string server_addr, const disconnect_event& disconnect_fun)
{
    int ret = 0;
    on_disconnect = disconnect_fun;
    addr_.sun_family = AF_UNIX;
    std::strcpy(addr_.sun_path, server_addr.c_str());
    
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
    int ret = 0;
    if ((sock_ = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        ret = -errno;
        std::cout << "udsock socket, create socket failed" << std::endl;
        return ret;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) 
    {
        ret = -errno;
        std::cout << "udsock setsockopt, set SO_RCVTIMEO failed" << std::endl;
        return ret;
    }

    for (int retry = 1; retry <= kReConnectCount; retry++)
    {
        if (connect(sock_, (struct sockaddr*)&addr_, sizeof(addr_)) == -1) 
        {
            std::cout << "connect server failed, retry count:" << retry << std::endl;
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
            clean_timeout_requeset();
            continue;
        }

        if (head.data_size == 0) continue;

        if (buffer_size_ < head.data_size)
        {
            if (!(recv_buff = (char*)malloc(head.data_size)))
            {
                std::cout << "domain socket, malloc failed" << std::endl;
                recv_buff = reserve_buff;
                continue;
            }
            is_free = true;
        }

        bzero(recv_buff, head.data_size);
        if (RecvBytes(recv_buff, head.data_size) < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                CleanSocket("recv body");
            clean_timeout_requeset();
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
    }

    if (is_free)
    {
        free(recv_buff);
    }

    std::cout << "udsocket client thread exit" << std::endl;
}

int UDSockClient::SendRequest(std::string& request, const async_result_cb& result_cbk)
{
    static unsigned long long request_id = 1;
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
    memcpy(send_buff + sizeof(RpcRequestHdr), request.c_str(), request.size());

    {
        std::lock_guard<std::mutex> _(lock_req_);
        head->id = request_id++;
        request_.insert(std::make_pair(head->id, value));
    }

    {
        std::lock_guard<std::mutex> _(lock_send_);
        if (SendBytes(send_buff, buff_size) == -1)
        {
            ret = -errno;
        }
    }
    free(send_buff);

    return ret;
}

void UDSockClient::Stop()
{
    running_ = false;
    CleanSocket("stop thread");
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
        errno = ENOTCONN;
        return -1;
    }

    buff += n;
    nbytes -= n;
    if (nbytes > 0) {
        goto again;
    }
    assert(nbytes == 0);
    return nbytes;
}

int64_t UDSockClient::SendBytes(const char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    n = send(sock_, (void*)buff, nbytes, MSG_NOSIGNAL);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            goto again;
        else
        {
            std::cout << "send1 errno = " << errno << std::endl;
            return -1;
        }
    } else if (n == 0) {
        std::cout << "send2 errno = " << errno << std::endl; 
        return -1;
    }

    buff += n;
    nbytes -= n;
    if (nbytes > 0) {
        goto again;
    }
    assert(nbytes == 0);
    return nbytes;
}
