#include <unistd.h>
#include <poll.h>
#include <sys/un.h>
#include <errno.h>
#include <assert.h>
#include <cstring>
#include "poll_server.h"

UDSockServer::UDSockServer() : lis_sock_(-1), buffer_size_(kBufferSize), running_(false)
{}

UDSockServer::~UDSockServer()
{
    CLOSE_FD(lis_sock_);
    unlink(address_.c_str());
}

bool UDSockServer::Init(const std::string& server_addr, const RequestCbk& on_request)
{
    address_ = server_addr;
    on_request_ = on_request;

    lis_sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == lis_sock_)
    {
        LOG_OUT("socket create failed" , strerror(errno));
        return false;
    }

    if (SetNonBlocking(lis_sock_) < 0)
    {
        LOG_OUT("SetNonBlocking failed" , strerror(errno));
        close(lis_sock_);
        return false;
    }

    sockaddr_un server_sockaddr;
    server_sockaddr.sun_family = AF_UNIX;
    strcpy(server_sockaddr.sun_path, address_.c_str());

    unlink(address_.c_str());

    if (-1 == bind(lis_sock_, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)))
    {
        LOG_OUT("bind failed" , strerror(errno));
        close(lis_sock_);
        unlink(address_.c_str());
        return false;
    }

    if (-1 == listen(lis_sock_, 5))
    {
        LOG_OUT("listen failed" , strerror(errno));
        close(lis_sock_);
        unlink(address_.c_str());
        return false;
    }

    // thread_ = std::thread(&UDSockServer::Run, this);

    return true;
}

bool UDSockServer::Accept(struct pollfd* fds, int& maxi, Buffer* buffs)
{
    int pos = -1;
    int cfd = accept(fds[0].fd, NULL, NULL);
    if (cfd == -1) 
    {
        LOG_OUT("accept failed", strerror(errno));
        return false;
    }

    int i = 1;
    for (; i < kMaxFiles; i++)
    {
        if (fds[i].fd == -1)
        {
            pos = i;
            break;
        }
    }

    if (i == kMaxFiles)
    {
        LOG_OUT("WARN", "too many clients");
        close(cfd);
        return false;
    }

    if (SetNonBlocking(cfd) < 0)
    {
        LOG_OUT("SetNonBlocking failed" , strerror(errno));
        close(cfd);
        return false;
    }

    if (i > maxi)
        maxi = i;

    fds[pos].fd = cfd;
    fds[pos].events = POLLIN;
    buffs[pos].AllocMem(buffer_size_);

    LOG_OUT("new client connected", "");
    return true;
}


int UDSockServer::Run()
{
    int nready = 0, maxi = 0;
    struct pollfd fds[kMaxFiles];
    Buffer buffs[kMaxFiles];

    for (int i = 0; i < kMaxFiles; i++)
        fds[i].fd = -1;

    fds[0].fd = lis_sock_;
    fds[0].events = POLLIN;
    running_ = true;

    while(running_)
    {
        nready = poll(fds, maxi + 1, -1);
        if (fds[0].revents & POLLIN)
        {
            --nready;
            Accept(fds, maxi, buffs);
        }
        for (int i = 1; i <= maxi && nready > 0; i++)
        {
            if (fds[i].fd == -1) continue;

            if (fds[i].revents & POLLIN)
            {
                char *s = buffs[i].s, *e = buffs[i].e, *buf = buffs[i].buf;
                --nready;
                int32_t pit_size = buffer_size_ - (e - buf);
                if (pit_size == 0) 
                {
                    std::cout << "s = " << *s << " e - s = " << (e - s) << " buf = " << *buf << " pit_size = " << pit_size << "buf size = " << buffer_size_ << std::endl;
                    sleep(1);
                    continue;
                }
                // std::cout << "s = " << *s << " e - s = " << (e - s) << " buf = " << *buf << " pit_size = " << pit_size << "buf size = " << buffer_size_ << std::endl;
                int nread = RecvData(fds[i].fd, e, pit_size);
                if (nread == -1)
                {
                    LOG_OUT("read head failed", std::to_string(errno));
                    CLOSE_FD(fds[i].fd);
                }
                else
                {
                    // 解析包体
                    e += nread;
                    while((e - s) > sizeof(RpcRequestHdr))
                    {
                        RpcRequestHdr* head = reinterpret_cast<RpcRequestHdr*>(s);
                        uint64_t req_size = head->data_size + sizeof(RpcRequestHdr);
                        if (req_size <= (e - s))
                        {
                            std::string data = on_request_(s, head->data_size);
                            head->data_size = data.size();
                            if (WriteVec(fds[i].fd, s, sizeof(RpcRequestHdr), s + sizeof(RpcRequestHdr), head->data_size) == -1)
                            {
                                LOG_OUT("send data failed", strerror(errno));
                                e = s = buf;
                                CLOSE_FD(fds[i].fd);
                                break;
                            }
                    
                            s += req_size;
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (fds[i].fd != -1)
                    {
                        memcpy(buf, s, e - s);
                        buffs[i].SavePos(buf, buf + (e - s));
                    }
                    else
                    {
                        buffs[i].Clean();
                    }
                }
            } 
            else if (fds[i].revents & (POLLERR | POLLHUP))
            {
                --nready;
                buffs[i].Clean();
                LOG_OUT("POLLERR | POLLHUP event", strerror(errno));
                CLOSE_FD(fds[i].fd);
            }
        }
    }

    for (int i = 0; i < kMaxFiles; i++)
    {
        CLOSE_FD(fds[i].fd);
        buffs[i].Clean();
    }
        
    LOG_OUT("udsocket server thread exit", "");

    return 0;
}

void UDSockServer::Stop()
{
    running_ = false;
    CLOSE_FD(lis_sock_);
    if (thread_.joinable())
    {
        thread_.join();
    }
}




