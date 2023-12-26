#ifndef _DOMAIN_COMMON_
#define _DOMAIN_COMMON_
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <cstring>
#include <assert.h>

#include <signal.h>

const int kBufferSize = 5120;
const std::string kServerAddress = "/tmp/unix.sock";

// server configure
const int kMaxFiles = 1024;

// client configure
const int kReConnectCount = 2;
const int kReconnectInterval = 1; // s
const uint64_t kCleanTimeoutRequest = 3000; // ms

struct RpcRequestHdr
{
    uint64_t id;
    uint32_t data_size;
    char data[];
};

#define CLOSE_FD(fd) \
    do  \
    {   \
        if ((fd) != -1) \
        { \
            close((fd));    \
            (fd) = -1;      \
        } \
    } while (0);


inline void LOG_OUT(const std::string& info, const std::string& str)
{
    std::cout << info << " " << str << std::endl;
}

class SockIO
{
public:
    struct Buffer
    {
    private:
        int fd;
        int size;
        char* buf = nullptr;
        char* s = nullptr;
        char* e = nullptr;
    
    public:
        Buffer(const int& size)
        {
            buf = new char[size];
            s = e = buf;
            fd = -1;
            this->size = size;
        }

        Buffer(const int& size, const int& fd)
        {
            buf = new char[size];
            s = e = buf;
            this->size = size;
            this->fd = fd;
        }

        ~Buffer()
        {
            Clean();
        }

        inline int Fd()
        {
            return fd;
        }

        inline int Size()
        {
            return size;
        }

        inline void Dig(const int& bytes)
        {
            s += bytes;
        }

        inline void Fill(const int& bytes)
        {
            e += bytes;
        }

        inline char* PitAddr()
        {
            return e;
        }

        inline int PitSize()
        {
            return size - (e - buf);
        }

        inline char* DataAddr()
        {
            return s;
        }

        inline int DataSize()
        {
            return e - s;
        }

        inline void SavePos(char* s, char* e)
        {
            this->s = s;
            this->e = e;
        }

        inline void Move()
        {
            int len = DataSize();
            if (len > 0)
            {
                memcpy(buf, s, len);
                SavePos(buf, buf + len);
            }
            else
            {
                ResetPos();
            }
        }

        inline void ResetPos()
        {
            e = s = buf;
        }

        inline void Expand(const int& size)
        {
            int len = DataSize();
            char* tbuf = new char[size];
            memcpy(tbuf, s, len);
            if (buf)
            {
                delete[] buf;
            }
            buf = tbuf;
            this->size = size;
            SavePos(buf, buf + len);
        }

        inline void Clean()
        {
            if(buf)
            {
                delete[] buf;
            }
            buf = nullptr;
            size = 0;
            ResetPos();
            CLOSE_FD(fd);
        }

    };

    int SetNonBlocking(int sockfd) 
    {
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) 
            return -errno;

        flags |= O_NONBLOCK;
        
        if (fcntl(sockfd, F_SETFL, flags) == -1) 
            return -errno;

        return 0;
    }

    int64_t WriteVec(int fd, void* head, int64_t hsize, void* body, int64_t bsize)
    {
        int64_t n = 0;
        struct iovec iov[2];
        iov[0].iov_base = head;
        iov[0].iov_len = hsize;
        iov[1].iov_base = body;
        iov[1].iov_len = bsize;
    again:
        n = writev(fd, iov, 2);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                goto again;
            else
                return -1;
        } else if (n == 0) {
            return -1;
        }
        assert(n == bsize + hsize);
        return n;
    }

    int64_t SendBytes(int fd, const char* buff, int64_t nbytes)
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

    int64_t RecvBytes(int fd, char* buff, int64_t nbytes)
    {
        int64_t n = 0;
    again:
        n = recv(fd, (void*)buff, 1, 0);
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

    int64_t RecvData(int fd, char* buff, int64_t size)
    {
        int64_t n = 0;
        n = recv(fd, (void*)buff, size, 0);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return 0;
            else
                return -1;
        } else if (n == 0) {
            return -1;
        }
        return n;
    }
};



#endif // _DOMAIN_COMMON_