#include "domain_client.h"
#include <unistd.h>
#include <assert.h>

void disconn_event()
{
    std::cout << "server quit...!!!" << std::endl;
}

int64_t diff_us(struct timespec& start, struct timespec& end)
{
    return (end.tv_sec - start.tv_sec) * 1000000 + ((end.tv_nsec - start.tv_nsec) / 1000); 
}

void do_respone(char* resp_buff, uint64_t size)
{
    assert(size == 1024);
}

int main()
{
    // 1KB, 1000000 reqs, 9549510 us
    std::string str_1K = "OT9jvg0dZvP36tZcKAPnBcRDg2FhYW0gGEdO7Chw7EyueGG68CTlYIliZDgv5Frp23rCiO3DX4gNAYjH3SVjDoplCquuRrMnVbm9d9CuF2cvSf2yPi7RlkiY5yo59Z734OS4T0v15jxRIcczdqTi4y4colDdMdK8R6nqG4JwDTJp77bP4614uXeDmnubqdpkCKcn9kBSfN6HTFJaNG2NzTnrd0y5jqBaaxL2lv134aku7DFoz7Re6d50SV9hPURJfaIusOjoJWBMqxa4aeSMAiwPHcbR2xFkNNCUxJE3W7D53iLxaS1hux4L9SEYQukiDttvjGc0HVQVaikPy2YPT7pjCtbJxVdi3dOp6uEAke4vgwNAM8oRIap20ETpH9tPtahjiII4uoGlk8t6JSj5gDBysJWAAMv65GSG5nWJGGXC22dl7MhoGh7TNZf4ZwvR4R9UjJeW7Cet086DGxBKkfUk29qbDFSM3uYzcisNdexe0j4B0eKgMHMvjAi7flX1dJnjKaMy1EvYYptPILDnISz2uSGRamwzdsSnTftDi5eBt2yzjobsacUNzM5jtgxDlk3qsogIZFfBXU3l3t8Aj0jLMMC6hOqeoUGMeBMAASsswGepwRzyzXWcSJbD5wuyxZkTvHp1AP39JEP6Qj6UfZq8X4mjN6oKHHf0GR0L6rpC7wdiRV3GtRnsUAK5h5BUjmMuexN8A8MKKt6iv36JIlIhglg2V70oaKVwyQh6erU5lCWwHYVmeJ90hA3hL1cyvS8h7pcXfOVOJ8jkAqmgP4WG7RqymKK7x3vqEBQM7VdU7DXFULKNRPMnSRylvvnoMFWAp0X1JOAz7Rg6HPreINPuiQRznf0Ob1RGy67TJS6kDXc9He2SB0BE3fTSKwN51rUdaApedh0M7FgjkTy5SXCJvazJlud8nlLajGn1vrdog7CRVuCwp6Skm9jXuiHKZkD4nO4mFObgMPTIN2B7WVp956Q38Xqq5d27rlnByRxeq9qaBNTE5zkxbQpooEK8";
    try
    {
        struct timespec begin, end; 
        UDSockClient client;
        if (client.Init(kServerAddress, &disconn_event) < 0)
        {
            perror("Init");
            return -1;
        }
        uint64_t max_cnt = 1000000;
        clock_gettime(CLOCK_REALTIME, &begin);
        for (uint64_t i = 0; i < max_cnt; i++)
        {
            client.SendRequest(str_1K, do_respone);
        }
        clock_gettime(CLOCK_REALTIME, &end);
        std::cout << "spend: " << diff_us(begin, end) << " us" << std::endl;
        client.Stop();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}