#include "network/Socket.h"
#include "network/Tcpclient.h"
#include <iostream>
#include <charconv>
#include <cstring>
#include <cerrno>
#include <arpa/inet.h>
#include <stdio.h>

int main(int argc, char const *argv[]) // 注意输入为 ./IMclient ip地址 port
{
    if (argc != 3)
    {
        std::cerr << "Usage: ./IMClient <address> <port>" << std::endl;
        return 1;
    }
    // 设计端口检查 使用from——chars函数
    const char *port_str = argv[2];
    int port = 0;
    auto [ptr, ec] = std::from_chars(port_str, port_str + std::strlen(port_str), port);
    if (ec == std::errc{} && ptr == port_str + std::strlen(port_str) && port >= 1 && port <= 65535)
    {
        std::cout << "端口正确" << std::endl;
    }
    else
    {
        std::cerr << "端口输入错误" << std::endl;
        return 1;
    }
    // 设计ip地址检查
    struct in_addr addr{}; // in_addr是一个专门存放二进制ip地址的一个结构体
    if (inet_pton(AF_INET, argv[1], &addr) != 1)
    {
        std::cerr << "ip地址输入错误!!!" << std::endl;
        return 1;
    }
    Tcpclient client(argv[1],port);
    return client.run();
}