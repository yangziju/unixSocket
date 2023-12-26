#include <sys/types.h>
#include <sys/epoll.h>
#include <errno.h>
#include <cstring>
#include <assert.h>
#include "poll_client.h"


UDSockClient::UDSockClient(const int& buffer_size)
    :buffer_size_(buffer_size), sock_(-1), running_(false) 
{

}

UDSockClient::~UDSockClient() 
{
    CLOSE_FD(sock_);
}

bool UDSockClient::Init(const std::string& server_addr, const OnDisconnct& on_disconn)
{
    on_disconn_ = on_disconn;
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
        CLOSE_FD(sock_);
        return false;
    }

    if (connect(sock_, (struct sockaddr*)&addr_, sizeof(addr_)) == -1) 
    {
        CLOSE_FD(sock_);
        return false;
    }

    thread_ = std::thread(&UDSockClient::Run, this);

    return true;
}

bool UDSockClient::ConnectServer()
{
    int tmp_sock;
    if ((tmp_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) 
    {
        return false;
    }

    if (SetNonBlocking(tmp_sock) < 0)
    {
        return false;
    }

    on_disconn_();

    for (int retry = 1; retry <= 10; retry++)
    {
        if (connect(tmp_sock, (struct sockaddr*)&addr_, sizeof(addr_)) == -1) 
        {
            std::cout << "connect server failed, retry count:" << retry << std::endl;
            CleanRequest();
            usleep(100000);
            continue;
        }
        sock_ = tmp_sock;
        return true;
    }
    return false;
}

void UDSockClient::Run()
{
    constexpr int kHeadSize = sizeof(RpcRequestHdr);
    int ev_cnt = 0, res = 0;
    Buffer buffer(buffer_size_);
    struct epoll_event ev;
    int efd = epoll_create(1);
    if (efd == -1)
    {
        perror("epoll_create");
        return;
    }

    ev.events = EPOLLIN;
    if (sock_ != -1)
    {
        ev.data.fd = sock_;
        res = epoll_ctl(efd, EPOLL_CTL_ADD, sock_, &ev);
        if (res == -1)
        {
            perror("epoll_ctl");
            return;
        }  
    }


    running_ = true;

    while(running_)
    {
        if (sock_ == -1)
        {
            if (ConnectServer())
            {
                res = epoll_ctl(efd, EPOLL_CTL_ADD, sock_, &ev);
                if (res == -1)
                {
                    perror("epoll_ctl");
                    return;
                }  
            }
            else 
                continue;
        }

        if (epoll_wait(efd, &ev, 1, 10) <= 0)
        {
            continue;
        }

        if (ev.events & (EPOLLERR | EPOLLHUP))
        {
            if (ev.events & EPOLLERR)
                LOG_OUT("POLLERR event", strerror(errno));
            else
                LOG_OUT("POLLHUP event", strerror(errno));

            res = epoll_ctl(efd, EPOLL_CTL_DEL, ev.data.fd, NULL);
            if (res == -1)
            {
                perror("EPOLL_CTL_DEL");
            }
            buffer.ResetPos();
            CLOSE_FD(sock_);
            continue;
        }
    
        if (ev.events & EPOLLIN)
        {
            int bytes = RecvData(sock_, buffer.PitAddr(), buffer.PitSize());
            if (bytes > 0)
            {
                buffer.Fill(bytes);
                while(buffer.DataSize() > kHeadSize)
                {
                    RpcRequestHdr* head = reinterpret_cast<RpcRequestHdr*>(buffer.DataAddr());
                    int total_size = head->data_size + kHeadSize;
                    if (total_size > buffer.Size())
                    {
                        buffer.Expand(total_size + 2 * kHeadSize);
                    }
                    if (total_size <= buffer.DataSize())
                    {
                        {
                            std::lock_guard<std::mutex> _(lock_req_);
                            auto it = request_.find(head->id);
                            if (it != request_.end())
                            {
                                it->second(buffer.DataAddr() + kHeadSize, head->data_size);
                                request_.erase(head->id);
                            }
                        }
                        buffer.Dig(total_size);
                    }
                    else
                    {
                        break;
                    }
                }
                buffer.Move();
            }
        }
    }

    std::cout << "udsocket client thread exit" << std::endl;
}

int UDSockClient::SendRequest(std::string& request, const ResponseCbk& response_cbk)
{
    static uint64_t request_id = 1;
    RpcRequestHdr head;
    ResponseCbk cbk = response_cbk;

    head.data_size = request.size();

    {
        std::lock_guard<std::mutex> _(lock_req_);
        head.id = request_id++;
        request_.insert(std::make_pair(head.id, cbk));
    }

    {
        std::lock_guard<std::mutex> _(lock_send_);
        if (WriteVec(sock_, &head, sizeof(RpcRequestHdr), (void*)request.c_str(), request.size()) == -1)
        {
            return -errno;
        }
    }

    return 0;
}

void UDSockClient::Stop()
{
    running_ = false;
    CLOSE_FD(sock_);
    if (thread_.joinable())
    {
        thread_.join();
    }
}

inline void UDSockClient::CleanRequest()
{
    std::lock_guard<std::mutex> _(lock_req_);
    request_.clear();
}

bool UDSockClient::IsConnected()
{
    return sock_ != -1;
}
