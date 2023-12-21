#include <iostream>
#include "domain_client.h"
int g_sleep_us = 0;
int g_cnt = 100;
void disconn_event()
{
    std::cout << "server quit...!!!" << std::endl;
}

void do_respone1(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id != expect_id)
        std::cout << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    if (expect_id % g_cnt == 0) 
        std::cout << "th1 success, id = " << expect_id << std::endl;
    // expect_id = recv_id + 1;
    expect_id++;
}

void loop_send1(UDSockClient& client)
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if(client.SendRequest(req, std::bind(&do_respone1, std::placeholders::_1, std::placeholders::_2)) < 0) 
        {
            std::cout << "th1 send failed, errno = " << errno << std::endl;
        }
        usleep(g_sleep_us);
    }
}

void do_respone2(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id != expect_id)
        std::cout << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    if (expect_id % g_cnt == 0) 
        std::cout << "th2 success, id = " << expect_id << std::endl;
    // expect_id = recv_id + 1;
    expect_id++;
}

void loop_send2(UDSockClient& client)
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if(client.SendRequest(req, std::bind(&do_respone2, std::placeholders::_1, std::placeholders::_2)) < 0) 
        {
            std::cout << "th2 send failed, errno = " << errno << std::endl;
        }
        usleep(g_sleep_us);
    }
}

void do_respone3(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id != expect_id) 
        std::cout << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    if (expect_id % g_cnt == 0) 
        std::cout << "th3 success, id = " << expect_id << std::endl;
    // expect_id = recv_id + 1;
    expect_id++;
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
            std::cout << "th3 send failed, errno = " << errno << std::endl;
        }
        usleep(g_sleep_us);
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2)
        {
            g_sleep_us = atoi(argv[1]);
        }
        UDSockClient client;
        const int th_nums = 3;
        if (client.Init(kServerAddress, &disconn_event) < 0)
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