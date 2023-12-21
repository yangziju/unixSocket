#include <unistd.h>
#include <sys/types.h>
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
    CLOSE_FD(sock_);
}

bool UDSockClient::Init(const std::string& server_addr, const OnDisconnct& on_disconn)
{
    on_disconn_ = on_disconn;
    addr_.sun_family = AF_UNIX;
    std::strcpy(addr_.sun_path, server_addr.c_str());

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
    int head_size = sizeof(RpcRequestHdr);
    Buffer buffer;
    buffer.AllocMem(buffer_size_);

    struct pollfd pfd;
    pfd.fd = sock_;
    pfd.events = POLLIN;
    running_ = true;

    while(running_)
    {
        if (sock_ == -1)
        {
            if (ConnectServer()) 
                pfd.fd = sock_;
            else 
                continue;
        }

        if (poll(&pfd, 1, 1) == -1)
        {
            std::cout << "poll ret = -1, errno: " << strerror(errno) << std::endl;
        }

        if (pfd.revents & POLLIN)
        {
            char *s = buffer.s, *e = buffer.e, *buf = buffer.buf;
            int32_t pit_size = buffer_size_ - (e - buf);
            if (pit_size == 0) 
            {
                std::cout << "s = " << *s << " e - s = " << (e - s) << " buf = " << *buf << " pit_size = " << pit_size << "buf size = " << buffer_size_ << std::endl;
                sleep(1);
                continue;
            }
            int nread = RecvData(sock_, e, pit_size);
            if (nread == -1)
            {
                LOG_OUT("read head failed", std::to_string(errno));
                CleanRequest();
                // CLOSE_FD(sock_);
            }
            else
            {
                e += nread;
                while((e - s) > head_size)
                {
                    RpcRequestHdr* head = reinterpret_cast<RpcRequestHdr*>(s);
                    int req_size = head->data_size + head_size;
                    if (req_size <= (e - s))
                    {
                        {
                            std::lock_guard<std::mutex> _(lock_req_);
                            auto it = request_.find(head->id);
                            if (it != request_.end())
                            {
                                it->second(s + head_size, head->data_size);
                                request_.erase(head->id);
                            }
                        }
                        s += req_size;
                    }
                    else
                    {
                        break;
                    }
                }
                memcpy(buf, s, e - s);
                buffer.SavePos(buf, buf + (e - s));
            }
        }
        
        if (pfd.revents & (POLLERR | POLLHUP))
        {
            if (pfd.revents & POLLERR)
                LOG_OUT("POLLERR event", strerror(errno));
            else
                LOG_OUT("POLLHUP event", strerror(errno));

            CleanRequest();
            buffer.SavePos(buffer.buf, buffer.buf);
            CLOSE_FD(sock_);
        }
    }

    buffer.Clean();

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
