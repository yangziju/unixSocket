#include <iostream>
#include "poll_client.h"
int g_sleep_us = 0;
int g_cnt = 100;
#define EABLEABLE_REQUET_LOG
#define ENABLE_TIPS_LOG
void disconn_event()
{
    std::cout << "[WARNING] server quit...!!!" << std::endl;
}

void do_respone1(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id == (10 * expect_id++)) {
        #ifdef ENABLE_TIPS_LOG
        if (expect_id % g_cnt == 0) 
            std::cout << "[SUCCESS] th1 id = " << expect_id << std::endl;
        #endif
    } else {
        std::cout << "[ERROR] th1 expect_id = " << expect_id << ", recv_id = " << data << std::endl;
    }
}

void loop_send1(UDSockClient& client)
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if(client.SendRequest(req, std::bind(&do_respone1, std::placeholders::_1, std::placeholders::_2)) < 0) 
        {
            #ifdef EABLEABLE_REQUET_LOG
            std::cout << "th1 send failed: " << strerror(errno)<< std::endl;
            #endif
        }
        usleep(g_sleep_us);
    }
}

void do_respone2(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id == (10 * expect_id++)) {
        #ifdef ENABLE_TIPS_LOG
        if (expect_id % g_cnt == 0) 
            std::cout << "[SUCCESS] th2 id = " << expect_id << std::endl;
        #endif
    } else {
        std::cout << "[ERROR] th2 expect_id = " << expect_id << ", recv_id = " << data << std::endl;
    }
}

void loop_send2(UDSockClient& client)
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if(client.SendRequest(req, std::bind(&do_respone2, std::placeholders::_1, std::placeholders::_2)) < 0) 
        {
            #ifdef EABLEABLE_REQUET_LOG
            std::cout << "th2 send failed: " << strerror(errno)<< std::endl;
            #endif
        }
        usleep(g_sleep_us);
    }
    client.Stop();
}

void do_respone3(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id == (10 * expect_id++)) {
        #ifdef ENABLE_TIPS_LOG
        if (expect_id % g_cnt == 0) 
            std::cout << "[SUCCESS] th3 id = " << expect_id << std::endl;
        #endif
    } else {
        std::cout << "[ERROR] th3 expect_id = " << expect_id << ", recv_id = " << data << std::endl;
    }
}

void loop_send3(UDSockClient& client)
{
    static uint64_t req_id = 0;
    int ret = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if((ret = client.SendRequest(req, std::bind(&do_respone3, std::placeholders::_1, std::placeholders::_2))) < 0) 
        {
            #ifdef EABLEABLE_REQUET_LOG
                std::cout << "th3 send failed: " << strerror(errno)<< std::endl;
            #endif
        }
        usleep(g_sleep_us);
    }
}

int main(int argc, char** argv)
{
    try
    {
        signal(SIGPIPE, SIG_IGN);
        if (argc == 2)
        {
            g_sleep_us = atoi(argv[1]);
        }
        UDSockClient client(sizeof(RpcRequestHdr) + 1);
        const int th_nums = 3;
        if (!client.Init(kServerAddress, &disconn_event))
        {
            perror("Init");
            return -1;
        }
        std::thread threads[th_nums];
        threads[0] = std::thread(&loop_send1, std::ref(client));
        threads[1] = std::thread(&loop_send2, std::ref(client));
        threads[2] = std::thread(&loop_send3, std::ref(client));

        for (int i = 0; i < th_nums; i++)
        {
            threads[i].join();
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}