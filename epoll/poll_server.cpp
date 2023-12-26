#include <sys/epoll.h>
#include <sys/un.h>
#include <errno.h>
#include <assert.h>
#include <cstring>
#include "poll_server.h"

const int kHeadSize = sizeof(RpcRequestHdr);

UDSockServer::UDSockServer(const int& buffer_size) : lis_sock_(-1), buffer_size_(buffer_size), running_(false)
{

}

UDSockServer::~UDSockServer()
{
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
        CLOSE_FD(lis_sock_);
        return false;
    }

    sockaddr_un server_sockaddr;
    server_sockaddr.sun_family = AF_UNIX;
    strcpy(server_sockaddr.sun_path, address_.c_str());

    unlink(address_.c_str());

    if (-1 == bind(lis_sock_, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)))
    {
        LOG_OUT("bind failed" , strerror(errno));
        CLOSE_FD(lis_sock_);
        unlink(address_.c_str());
        return false;
    }

    if (-1 == listen(lis_sock_, 5))
    {
        LOG_OUT("listen failed" , strerror(errno));
        CLOSE_FD(lis_sock_);
        unlink(address_.c_str());
        return false;
    }

    // thread_ = std::thread(&UDSockServer::Run, this);

    return true;
}

bool UDSockServer::Accept(int efd, int fd, std::unordered_map<int, Buffer*>& conn)
{
    struct epoll_event tep;
    int res = 0;
    int cfd = accept(fd, NULL, NULL);
    if (cfd == -1) 
    {
        perror("accept");
        return false;
    }

    Buffer* buf = new Buffer(buffer_size_, cfd);
    tep.events = EPOLLIN; 
    tep.data.ptr = (void*)buf;
    res = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &tep);
    if (res == -1)
    {
        perror("EPOLL_CTL_ADD new conn");
        delete buf;
        return false;
    }
    conn[cfd] = buf;

    std::cout << "new client connected" << std::endl;
    return true;
}


int UDSockServer::Run()
{
    struct epoll_event tep, events[kMaxFiles];
    std::unordered_map<int, Buffer*> conn;
    int res = 0, event_cnt = 0;
    int efd = epoll_create(kMaxFiles);
    if (efd == -1)
    {
        perror("epoll_create");
        return -1;
    }
    
    Buffer* tbuf = new Buffer(buffer_size_, lis_sock_);
    tep.events = EPOLLIN;
    tep.data.ptr = tbuf;
	res = epoll_ctl(efd, EPOLL_CTL_ADD, lis_sock_, &tep);
    if (res == -1)
    {
        perror("epoll_ctl");
        delete tbuf;
        return -1;
    }
    conn[lis_sock_] = tbuf;

    running_ = true;

    while(running_)
    {
        event_cnt = epoll_wait(efd, events, kMaxFiles, 10);
        // std::cout << "event_cnt = " << event_cnt << std::endl;
        for (int i = 0; i < event_cnt; i++)
        {
            Buffer* buf = reinterpret_cast<Buffer*>(events[i].data.ptr);

            if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                if (events[i].events & EPOLLERR)
                    LOG_OUT("EPOLLERR event", strerror(errno));
                else
                    LOG_OUT("EPOLLHUP event", strerror(errno));

                res = epoll_ctl(efd, EPOLL_CTL_DEL, buf->Fd(), NULL);
                if (res == -1)
                {
                    perror("EPOLL_CTL_DEL");
                }

                delete buf;
                continue;
            }

            if ((buf->Fd() == lis_sock_) && (events[i].events & EPOLLIN))
            {
                Accept(efd, buf->Fd(), conn);
                continue;
            }

            if (events[i].events & EPOLLIN)
            {
                int bytes = RecvData(buf->Fd(), buf->PitAddr(), buf->PitSize());
                // std::cout << "1 fd: " << buf->Fd() << " pid_size: " << buf->PitSize() << " data_size: " << buf->DataSize() << " bytes: " << bytes  << std::endl;
                if (bytes > 0)
                {
                    buf->Fill(bytes);
                    // std::cout << "2 fd: " << buf->Fd() << " pid_size: " << buf->PitSize() << " data_size: " << buf->DataSize() << " bytes: " << bytes  << std::endl;
                    while(buf->DataSize() > kHeadSize)
                    {
                        RpcRequestHdr* head = reinterpret_cast<RpcRequestHdr*>(buf->DataAddr());
                        int32_t total_size = head->data_size + kHeadSize;
                        if (total_size > buf->Size())
                        {
                            buf->Expand(total_size + 2 * kHeadSize);
                        }
                        if (total_size <= buf->DataSize())
                        {

                            // std::cout << "3 fd: " << buf->Fd() << " total_size: " << total_size << " pid_size: " << buf->PitSize() << " data_size: " << buf->DataSize() << " bytes: " << bytes  << std::endl;
                            std::string data = on_request_(buf->DataAddr() + kHeadSize, head->data_size);
                            head->data_size = data.size();
                            if (WriteVec(buf->Fd(), (void*)head, kHeadSize, (void*)data.c_str(), data.size()) == -1)
                            {
                                std::cout << "send data failed: " << strerror(errno) << std::endl;
                                buf->ResetPos();
                                break;
                            }
                    
                            buf->Dig(total_size);
                            // std::cout << "4 fd: " << buf->Fd() << " total_size: " << total_size << " pid_size: " << buf->PitSize() << " data_size: " << buf->DataSize() << " bytes: " << bytes  << std::endl;
                        }
                        else
                        {
                            break;
                        }
                    }
                    buf->Move();
                    // std::cout << "5 fd: " << buf->Fd() << " pid_size: " << buf->PitSize() << " data_size: " << buf->DataSize() << " bytes: " << bytes  << std::endl;
                }
                else
                {
                    std::cout << "recv failed: bytes = " << bytes  << " " << strerror(errno) << std::endl;
                }
            }
        }
    }
    close(efd);
    for (auto it = conn.begin(); it != conn.end(); it++)
    {
        if (it->second)
        {
            delete it->second;
        }
    }
    LOG_OUT("udsocket server thread exit", "");
    return 0;
}

void UDSockServer::Stop()
{
    running_ = false;
    CLOSE_FD(lis_sock_);
    unlink(address_.c_str());
    if (thread_.joinable())
    {
        thread_.join();
    }
}

