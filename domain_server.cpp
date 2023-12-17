#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include "domain_server.h"

UDSockServer::UDSockServer() : lis_sock_(-1), cli_sock_(-1), buffer_size_(kBufferSize), running_(false)
{}

UDSockServer::~UDSockServer()
{
    if (lis_sock_ != -1)
    {
        close(lis_sock_);
    }
    if (cli_sock_ != -1)
    {
        close(cli_sock_);
    }
    unlink(address_.c_str());
}

int UDSockServer::Init(const std::string server_addr, response_fun on_response)
{
    address_ = server_addr;
    on_response_ = on_response;
    int ret = 0;

    lis_sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (-1 == lis_sock_)
    {
        return -errno;
    }

    int flags = fcntl(lis_sock_, F_GETFL, 0);
    if (flags == -1)
    {
        return -errno;
    }

    if (-1 == fcntl(lis_sock_, F_SETFL, flags | O_NONBLOCK))
    {
        return -errno;
    }

    sockaddr_un server_sockaddr;
    server_sockaddr.sun_family = AF_UNIX;
    strcpy(server_sockaddr.sun_path, address_.c_str());

    unlink(address_.c_str());

    if (-1 == bind(lis_sock_, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)))
    {
        ret = -errno;
        close(lis_sock_);
        unlink(address_.c_str());
        return ret;
    }

    if (-1 == listen(lis_sock_, 5))
    {
        ret = -errno;
        close(lis_sock_);
        unlink(address_.c_str());
        return ret;
    }

    thread_ = std::thread(&UDSockServer::Run, this);

    return 0;
}

int UDSockServer::Run()
{
    RpcRequestHdr head;
    std::string resp_data;
    bool free_recv_buff = false, free_resp_buff = false;
    char *resp_buff = nullptr, *recv_buff = nullptr, *reserve_buff = nullptr;

    running_ = true;
    if (!(reserve_buff = (char*)malloc(buffer_size_)))
    {
        return -errno;
    }

    recv_buff = reserve_buff;
    resp_buff = reserve_buff;

    while(running_)
    {
        if(free_recv_buff)
        {
            free(recv_buff);
            recv_buff = reserve_buff;
            free_recv_buff = false;
        }

        if(free_resp_buff)
        {
            free(resp_buff);
            resp_buff = reserve_buff;
            free_resp_buff = false;
        }
        bzero(reserve_buff, buffer_size_);
        
        if (cli_sock_ == -1)
        {
            std::cout << "waitting for client to connect" << std::endl;
            cli_sock_ = accept(lis_sock_, NULL, NULL);
            if (cli_sock_ == -1) {
                sleep(1);
                continue;
            }
            std::cout << "client connected" << std::endl;
        }

        bzero(&head, sizeof(RpcRequestHdr));
        if (RecvBytes((char*)&head, sizeof(RpcRequestHdr)) == -1)
        {
            CleanSocket();
            continue;
        }

        if (head.data_size == 0) continue;

        if (buffer_size_ < head.data_size)
        {
            recv_buff = (char*)malloc(head.data_size);
            assert(recv_buff);
            free_recv_buff = true;
        }

        bzero(recv_buff, head.data_size);
        if (RecvBytes(recv_buff, head.data_size) == -1) 
        {
            CleanSocket();
            continue;
        }
        
        resp_data = on_response_((char*)recv_buff, head.data_size);

        uint64_t resp_total_size = sizeof(RpcRequestHdr) + resp_data.size();
        if (buffer_size_ < resp_total_size)
        {
            if (!(resp_buff = (char*)malloc(resp_total_size))) {
                std::cout << "domain socket, malloc failed" << std::endl;
                resp_buff = reserve_buff;
                continue;
            }
            free_resp_buff = true;
        }
    
        bzero(resp_buff, resp_total_size);
        head.data_size = resp_data.size();
        memcpy(resp_buff, &head, sizeof(RpcRequestHdr));
        memcpy(resp_buff + sizeof(RpcRequestHdr), resp_data.c_str(), resp_data.size());
        
        if (SendBytes(resp_buff, resp_total_size) == -1)
        {
            CleanSocket();
        }
    }
    if (reserve_buff)
    {
        free(reserve_buff);
        reserve_buff = nullptr;
    }

    if(free_recv_buff)
    {
        free(recv_buff);
    }

    if(free_resp_buff)
    {
        free(resp_buff);
    }

    std::cout << "udsocket server thread exit" << std::endl;

    return 0;
}

void UDSockServer::Stop()
{
    running_ = false;
    CleanSocket();
    if (lis_sock_ != -1)
    {
        close(lis_sock_);
    }
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void UDSockServer::CleanSocket()
{
    if (cli_sock_ != -1)
    {
        close(cli_sock_);
        cli_sock_ = -1;
    }
}

int64_t UDSockServer::RecvBytes(char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    n = recv(cli_sock_, (void*)buff, nbytes, 0);
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

int64_t UDSockServer::SendBytes(const char* buff, int64_t nbytes)
{
    int64_t n = 0;
again:
    n = send(cli_sock_, (void*)buff, nbytes, MSG_NOSIGNAL);
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
