#include "network/Socket.h"
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
    std::cout << "Server address:" << argv[1] << ":" << port << std::endl;
    // 连接服务端，需要存放服务端的地址
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = addr;
    // 创建客户端套接字
    Socket client_socket(socket(AF_INET, SOCK_STREAM, 0));
    if (client_socket.get_socketfd() < 0)
    {
        perror("client_socket:");
        return 1;
    }
    // 用客户端去连接
    int ret = connect(client_socket.get_socketfd(), (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        perror("connect:");
        return 1;
    }
    std::cout << "Connected to server:" << argv[1] << ":" << port << std::endl;

    // 发送一条信息
    std::string massege;
    while (1) // 会把输入的字符读取一行放到massege中
    {
        std::cout << "send massege>:";
        //
        if (!std::getline(std::cin, massege)) { break; }
        // 如果等于的话，表示要退出
        if (massege == "/quit") { break; }
        // 不为quit，要在后面加'\n'保留一行数据的结束标记。
        massege.push_back('\n');
        // 在打印发送的信息
        std::cout << "Ready to send: " << massege << std::endl;
        // 使用send发送信息，需要使用offset指针偏移
        // 例如：当内核缓存区只吞下了400个字节，还差600个字节
        // 这个时候offset就会指向400的位置
        size_t offset = 0;                  // 定义指针偏移
        while (offset < massege.size()) // 小于说明信息没发送完成,继续完成发送
        {
            // send函数返回发送成功的字节
            ssize_t n = send(client_socket.get_socketfd(),
                             massege.data() + offset,
                             massege.size() - offset,
                             MSG_NOSIGNAL);
            if (n > 0)
            {
                offset += static_cast<size_t>(n);
            }
            else if (n < 0 && errno == EINTR)
            {
                continue;
            }
            else if (n == 0)
            {
                std::cerr << "send:no bytes sent\n";
                return 1;
            }
            else
            {
                perror("send:");
                return 1;
            }
        }
    }

    return 0;
}