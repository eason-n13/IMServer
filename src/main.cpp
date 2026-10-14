#include "network/Socket.h"
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

    std::cout << "Listen port:" << port << std::endl;
    // 创建socket对象
    Socket listen_socket(socket(AF_INET, SOCK_STREAM, 0));
    if (listen_socket.get_socketfd() < 0)
    {
        perror("socket:");
        return 1;
    }
    std::cout << "Socket created successfully" << std::endl;
    // 对创建的监听socket进行绑定
    struct sockaddr_in listen_sockaddr{}; // 创建一个监听结构体
    listen_sockaddr.sin_family = AF_INET;
    listen_sockaddr.sin_port = htons(port); // htons:主机字节序转网络字节序；atoi：ascll转int
    listen_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int ret = bind(listen_socket.get_socketfd(), (struct sockaddr *)&listen_sockaddr, sizeof(listen_sockaddr));
    if (ret < 0)
    {
        perror("bind:");
        return 1;
    }
    // 对绑定的服务端端口进行监听
    ret = listen(listen_socket.get_socketfd(), 128);
    if (ret < 0)
    {
        perror("listen:");
        return 1;
    }
    // 创建客户端来接受客户端请求
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    Socket client_fd(accept(listen_socket.get_socketfd(), (struct sockaddr *)&client_addr, &client_len));
    if (client_fd.get_socketfd() < 0)
    {
        perror("accept:");
        return 1;
    }
    //将接收到的客户端信息打印出来
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET,&client_addr,ip,sizeof(ip));
    std::cout << "Client connected: " << ip << ":" << ntohs(client_addr.sin_port) << std::endl;
    return 0;
}
