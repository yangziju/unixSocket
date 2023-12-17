#include "domain_client.h"

void disconn_event()
{
    std::cerr << "server quit...!!!" << std::endl;
}

std::string generateRandomString(int minLength, int maxLength) {

  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

  const int charsetSize = sizeof(charset) - 1;

  std::srand(std::time(0));

  int length = std::rand() % (maxLength - minLength + 1) + minLength;

  std::string randomString;

  for (int i = 0; i < length; ++i) {
    randomString += charset[std::rand() % charsetSize];
  }

  return randomString;
}

void do_respone(char* resp_buff, uint64_t size)
{
    static uint64_t expect_id = 0;
    if (++expect_id % 100 == 0) 
        std::cout << std::string(resp_buff, size) << std::endl;
}

void loop_send(UDSockClient& client)
{
    for(int i = 0; i < 1000; i++)
    {
        std::string req = generateRandomString(kBufferSize - 3, kBufferSize + 3);
        if(client.SendRequest(req, do_respone) < 0) 
        {
            perror("send req failed");
        }
        usleep(10000);
    }
}

int main() {

    try
    {
        UDSockClient client;
        const int th_nums = 3;
        if (client.Init(kServerAddress, &disconn_event) < 0)
        {
            perror("Init");
            return 1;
        }
        std::thread threads[th_nums];
        threads[0] = std::thread(&loop_send);
        threads[1] = std::thread(&loop_send);
        threads[2] = std::thread(&loop_send);
        sleep(5);
        client.Stop();
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