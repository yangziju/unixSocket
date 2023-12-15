#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <thread>
#include <map>
#include <assert.h>
#include "domainCommon.h"

using namespace std;

class UDSockServer 
{
public:
    UDSockServer() : lis_sock_(-1), cli_sock_(-1), buffer_size_(5120), running_(false)
    {}

    ~UDSockServer()
    {
        close(lis_sock_);
        close(cli_sock_);
        unlink(address_.c_str());
    }

    int Init(const std::string server_addr, response_fun on_response)
    {
        address_ = server_addr;
        on_response_ = on_response;

        lis_sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (-1 == lis_sock_)
        {
            return -errno;
        }

        sockaddr_un server_sockaddr;
        server_sockaddr.sun_family = AF_UNIX;
        strcpy(server_sockaddr.sun_path, address_.c_str());

        unlink(address_.c_str());

        if (-1 == bind(lis_sock_, (struct sockaddr*)&server_sockaddr, sizeof(server_sockaddr)))
        {
            close(lis_sock_);
            unlink(address_.c_str());
            return -errno;
        }

        listen(lis_sock_, 5);
        // thread_ = std::thread(&UDSockServer::Run, this);

        return 0;
    }

    int Run()
    {
        RpcRequestHdr head;
        char* resp_buff = nullptr, *recv_buff = nullptr;

        running_ = true;
        bool is_free = false;
        if (!(recv_buff = (char*)malloc(buffer_size_)))
        {
            return -errno;
        }
        resp_buff = recv_buff;

        while(running_)
        {
            if (cli_sock_ == -1)
            {
                cli_sock_ = accept(lis_sock_, NULL, NULL);
                if (cli_sock_ == -1) {
                    continue;
                }
                std::cout << "accept new connect" << std::endl;
            }

            // 读取头部数据
            if (RecvBytes(&head, sizeof(RpcRequestHdr)) <= 0)
            {
                Disconnect("recv head");
                continue;
            }

            // 解析数据体
            if (RecvBytes(recv_buff, head.data_size) <= 0) 
            {
                Disconnect("recv body");
                continue;
            }
            std::string resp_data = on_response_((char*)recv_buff, head.data_size);

            uint64_t resp_total_size = sizeof(RpcRequestHdr) + resp_data.size();
            if (buffer_size_ < resp_total_size)
            {
                resp_buff = (char*)malloc(resp_total_size);
                if (!resp_buff) {
                    Disconnect("malloc failed");
                    continue;
                }
                is_free = true;
            }

            head.data_size = resp_data.size();
            head.data = resp_buff + sizeof(RpcRequestHdr);
            memcpy(resp_buff, &head, sizeof(RpcRequestHdr));
            memcpy(resp_buff + sizeof(RpcRequestHdr), resp_data.c_str(), resp_data.size());
            

            if (SendBytes((void*)resp_buff, sizeof(RpcRequestHdr) + resp_data.size()) <= 0)
            {
                Disconnect("Send data");
            }

            if(is_free)
            {
                free(resp_buff);
                is_free = false;
                resp_buff = recv_buff;
            }
        }
        if (recv_buff)
        {
            free(recv_buff);
            recv_buff = nullptr;
        }
        return 0;
    }

    void stop()
    {
        running_ = false;
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

protected:

    void Disconnect(std::string fun_name)
    {
        std::cout << fun_name <<  ": Client disconnected, errno = " << errno << std::endl;
        close(cli_sock_);
        cli_sock_ = -1;
    }

    ssize_t RecvBytes(void* buff, size_t nbytes)
    {
        ssize_t n = 0;
again:
        if ((n = recv(cli_sock_, buff, nbytes, MSG_NOSIGNAL)) == -1) {
            if (errno == EINTR)
                goto again;
            else
                return -1;
        }
        if (n) assert(n == nbytes);
        return n;
    }

    ssize_t SendBytes(const void* buff, size_t nbytes)
    {
        ssize_t n = 0;
again:
        if ((n = send(cli_sock_, buff, nbytes, MSG_NOSIGNAL)) == -1) {
            if (errno == EINTR)
                goto again;
            else
                return -1;
        }
        if(n) assert(n == nbytes);
        return n;
    }

private:
    int lis_sock_;
    int cli_sock_;
    uint32_t buffer_size_;
    std::thread thread_;
    bool running_;
    std::string address_;
    response_fun on_response_;
};

std::string do_sponse(char* data, uint64_t size)
{
    data[size] = '\0';
    std::cout << data << std::endl;
    return "I am from server";
}

// std::string do_sponse(char* data, uint64_t size)
// {
//     RequestHead* head = reinterpret_cast<RequestHead*>(data);
//     switch (head->type)
//     {
//     case ADD:
//     {
//         AddRequest* add_req = reinterpret_cast<AddRequest*>(data + sizeof(RequestHead));
//         return do_add(add_req);
//     }
//     case DEL:
//     {
//         DelRequest* del_req = reinterpret_cast<DelRequest*>(data + sizeof(RequestHead));
//         return do_del(del_req);
//     }
//     default:
//         std::cout << "other.." << std::endl;
//         break;
//     }
// }

int main()
{
    UDSockServer server;
    if (server.Init(kServerAddress, do_sponse) < 0)
    {
        perror("init");
    }
    server.Run();
    return 0;
}