#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <poll.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
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

bool UDSockServer::Init(const std::string& server_addr, const ResultCbk& on_response)
{
    address_ = server_addr;
    on_response_ = on_response;

    lis_sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == lis_sock_)
    {
        LOG_OUT("socket create failed" , strerror(errno));
        return false;
    }

    int flags = fcntl(lis_sock_, F_GETFL, 0);
    if (flags == -1)
    {
        LOG_OUT("F_GETFL failed" , strerror(errno));
        close(lis_sock_);
        return false;
    }

    if (-1 == fcntl(lis_sock_, F_SETFL, flags | O_NONBLOCK))
    {
        LOG_OUT("F_SETFL failed" , strerror(errno));
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

bool Accept(struct pollfd* fds, int& maxi)
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

    int flags = fcntl(cfd, F_GETFL, 0);
    if (flags == -1)
    {
        LOG_OUT("F_GETFL failed" , strerror(errno));
        close(cfd);
        return false;
    }

    if (-1 == fcntl(cfd, F_SETFL, flags | O_NONBLOCK))
    {
        LOG_OUT("F_SETFL failed" , strerror(errno));
        close(cfd);
        return false;
    }

    if (i > maxi)
        maxi = i;
    fds[pos].fd = cfd;
    fds[pos].events = POLLIN;

    LOG_OUT("client connected", "");
    return true;
}

int UDSockServer::Run()
{
    struct RpcRequestHdr head;
    struct pollfd fds[kMaxFiles];
    for (int i = 0; i < kMaxFiles; i++)
        fds[i].fd = -1;
    fds[0].fd = lis_sock_;
    fds[0].events = POLLIN;
    int nready = 0, maxi = 0;
    char* buffer = new char[buffer_size_];
    running_ = true;

    while(running_)
    {
        nready = poll(fds, maxi + 1, -1);
        if (fds[0].revents & POLLIN)
        {
            --nready;
            Accept(fds, maxi);
        }
        for (int i = 1; i <= maxi && nready > 0; i++)
        {
            if (fds[i].fd == -1) continue;

            if (fds[i].revents & POLLIN)
            {
                --nready;
                int ret = RecvBytes(fds[i].fd, (char*)&head, sizeof(RpcRequestHdr));
                if (ret == -1)
                {
                    LOG_OUT("read head failed", std::to_string(errno));
                    CLOSE_FD(fds[i].fd);
                }
                if (ret != -1 && head.data_size > 0)
                {
                    if (RecvBytes(fds[i].fd, buffer, head.data_size) != -1)
                    {
                        std::string data = on_response_(buffer, head.data_size);
                        head.data_size = data.size();
                        memcpy(buffer, &head, sizeof(RpcRequestHdr));
                        memcpy(buffer + sizeof(RpcRequestHdr), data.c_str(), data.size());
                        if(SendBytes(fds[i].fd, buffer, data.size() + sizeof(RpcRequestHdr)) < 0)
                        {
                            LOG_OUT("send data failed", strerror(errno));
                            CLOSE_FD(fds[i].fd);
                        }
                    } else {
                        LOG_OUT("read body failed", strerror(errno));
                        CLOSE_FD(fds[i].fd);
                    }
                }
            } 
            else if (fds[i].revents & (POLLERR | POLLHUP))
            {
                --nready;
                LOG_OUT("POLLERR | POLLHUP event", strerror(errno));
                CLOSE_FD(fds[i].fd);
            }
        }
    }
    if (buffer)
        delete buffer;

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

int64_t UDSockServer::RecvBytes(int fd, char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    n = recv(fd, (void*)buff, nbytes, 0);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            goto again;
        else
            return -1;
    } else if (n == 0) {
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

int64_t UDSockServer::SendBytes(int fd, const char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    n = send(fd, (void*)buff, nbytes, MSG_NOSIGNAL);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            goto again;
        else
            return -1;
    } else if (n == 0) {
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