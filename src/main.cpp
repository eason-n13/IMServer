#include "network/Socket.h"
#include "network/Tcpserver.h"
#include <iostream>
#include <charconv>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./IMServer <port>" << std::endl;
        return 1;
    }
    // 设计端口检查 使用from——chars函数
    const char *port_str = argv[1];
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

    //绑定端口服务
    Tcpserver server(port);

    return server.run();
}
