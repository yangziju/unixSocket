#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <cstring>
#include <assert.h>
#include "poll_client.h"


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

    // if (setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) == -1) 
    // {
    //     ret = -errno;
    //     CleanSocket("init");
    //     return ret;
    // }

    if (SetNonBlocking(sock_) < 0)
    {
        ret = -errno;
        CleanSocket("set socket noblock");
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
    int ret = 0, tmp_sock;
    if ((tmp_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        ret = -errno;
        std::cout << "udsock socket, create socket failed" << std::endl;
        return ret;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (SetNonBlocking(tmp_sock) < 0)
    {
        ret = -errno;
        CleanSocket("set socket noblock");
        return ret;
    }

    on_disconnect();
    for (int retry = 1; retry <= kReConnectCount; retry++)
    {
        if (connect(tmp_sock, (struct sockaddr*)&addr_, sizeof(addr_)) == -1) 
        {
            std::cout << "connect server failed, retry count:" << retry << std::endl;
            sleep(kReconnectInterval);
            clean_timeout_requeset();
            continue;
        }
        sock_ = tmp_sock;
        return 0;
    }
    return -ECONNREFUSED;
}

void UDSockClient::Run()
{
    RpcRequestHdr head;
    char* buffer = new char[buffer_size_];

    struct pollfd pfd;
    pfd.fd = sock_;
    pfd.events = POLLIN;
    running_ = true;

    while(running_)
    {
        if (sock_ == -1)
        {
            if (ConnectServer() < 0) 
                continue;
            else 
                pfd.fd = sock_;
        }

        if (poll(&pfd, 1, -1) == -1)
        {
            CLOSE_FD(sock_);
            clean_timeout_requeset();
            continue;
        }

        if (pfd.revents & POLLIN)
        {
            int ret = RecvBytes(sock_, (char*)&head, sizeof(RpcRequestHdr));
            if (ret == -1)
            {
                LOG_OUT("read head failed", std::to_string(errno));
                CLOSE_FD(sock_);
            }
            if (ret != -1 && head.data_size > 0)
            {
                if (RecvBytes(sock_, buffer, head.data_size) != -1)
                {
                    {
                        std::lock_guard<std::mutex> _(lock_req_);
                        auto it = request_.find(head.id);
                        if (it != request_.end())
                        {
                            it->second.cbk(buffer, head.data_size);
                            request_.erase(head.id);
                        }
                    }
                } else {
                    LOG_OUT("read body failed", strerror(errno));
                    CLOSE_FD(sock_);
                }
            }
        }
        else if (pfd.revents & (POLLERR | POLLHUP))
        {
            LOG_OUT("POLLERR | POLLHUP", strerror(errno));
            clean_timeout_requeset();
            CLOSE_FD(sock_);
        }
    }

    if (buffer)
        delete buffer;

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
        if (SendBytes(sock_, send_buff, buff_size) == -1)
        {
            ret = -errno;
        }
    }
    free(send_buff);

    return ret;
}

// int UDSockClient::SendData(std::string& data, const )

void UDSockClient::Stop()
{
    running_ = false;
    CleanSocket("stop thread");
    if (thread_.joinable())
    {
        thread_.join();
    }
}


inline uint64_t diff_ms(const struct timespec& start, const struct timespec& now)
{
    return (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
}

void UDSockClient::clean_timeout_requeset()
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
    if (sock_ != -1)
    {
        std::cout << str << " close fd, errno = " << errno << std::endl;
        close(sock_);
        sock_ = -1;
    }
}

int64_t UDSockClient::RecvBytes(int fd, char* buff, int64_t nbytes)
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

int64_t UDSockClient::SendBytes(int fd, const char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    n = send(sock_, (void*)buff, nbytes, MSG_NOSIGNAL);
    LOG_OUT("send bytes = " + std::to_string(nbytes), std::to_string(errno));
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            goto again;
        else
        {
            return -1;
        }
    } else if (n == 0) {
        return -1;
    }
    sleep(1);
    buff += n;
    nbytes -= n;
    if (nbytes > 0) {
        goto again;
    }
    assert(nbytes == 0);
    return nbytes;
}
