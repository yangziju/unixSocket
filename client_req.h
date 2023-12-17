#include <iostream>
#include "domain_client.h"

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
    if (expect_id % 1000 == 0) 
        std::cout << "th1 success, id = " << expect_id << std::endl;
    expect_id = recv_id + 1;
}

void loop_send1(UDSockClient& client)
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if(client.SendRequest(req, do_respone1) < 0) 
        {
            std::cout << "th1 send failed, errno = " << errno << std::endl;
        }
        // usleep(10);
        // sleep(3);
    }
}

void do_respone2(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id != expect_id)
        std::cout << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    if (expect_id % 1000 == 0) 
        std::cout << "th2 success, id = " << expect_id << std::endl;
    expect_id = recv_id + 1;
}

void loop_send2(UDSockClient& client)
{
    static uint64_t req_id = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if(client.SendRequest(req, do_respone2) < 0) 
        {
            std::cout << "th2 send failed, errno = " << errno << std::endl;
        }
        // usleep(10);
        // sleep(3);
    }
}

void do_respone3(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    std::string data(resp_buff, size);
    uint64_t recv_id = std::stoll(data);
    if (recv_id != expect_id) 
        std::cout << "expect_id = " << expect_id << ", recv_id = " << recv_id  << ", resp_buff = " << resp_buff << std::endl;
    if (expect_id % 1000 == 0) 
        std::cout << "th3 success, id = " << expect_id << std::endl;
    expect_id = recv_id + 1;
}

void loop_send3(UDSockClient& client)
{
    static uint64_t req_id = 0;
    int ret = 0;
    while(1)
    {
        std::string req(std::to_string(req_id++));
        if((ret = client.SendRequest(req, do_respone3)) < 0) 
        {
            std::cout << "th3 send failed, errno = " << errno << std::endl;
        }
        // usleep(10);
        // sleep(2);
    }
}