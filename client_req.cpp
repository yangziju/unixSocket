#include "client_req.h"

int main()
{
    try
    {
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