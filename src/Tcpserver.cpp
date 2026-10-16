#include "network/Tcpserver.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

// 在这个函数实现Tcpserver.h的各个功能
// 首先实现构造函数
Tcpserver::Tcpserver(int port) : port_(port), listen_socket_(socket(AF_INET, SOCK_STREAM, MSG_NOSIGNAL)) // 传入端口和创建监听套接字
{
}

// 实现run函数
int Tcpserver::run()
{
    // 首先判断listen_socket_初始化是否成功
    if (listen_socket_.get_socketfd() < 0)
    {
        perror("socket:");
        return 1;
    }
    // 然后设置地址复用，防止服务端断开，重连时处于tcp断开后的TIME_WAIT
    // 从而导致  bind: Address already in use
    int enable = 1;
    int ret = setsockopt(listen_socket_.get_socketfd(),
                         SOL_SOCKET, SO_REUSEADDR, // SO_REUSEADDR对TIME_WAIT端口复用
                         &enable, sizeof(enable));
    if (ret < 0)
    {
        perror("setsockopt");
        return 1;
    }
    // 然后进行run函数中的bind
    struct sockaddr_in listen_socketaddr{};
    listen_socketaddr.sin_family = AF_INET;
    listen_socketaddr.sin_port = htons(port_);
    listen_socketaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ret = bind(listen_socket_.get_socketfd(),
               (struct sockaddr *)&listen_socketaddr,
               sizeof(listen_socketaddr));
    if (ret < 0)
    {
        perror("bind:");
        return 1;
    }
    // 然后进行listen
    ret = listen(listen_socket_.get_socketfd(), 128);
    if (ret < 0)
    {
        perror("listen:");
        return 1;
    }
    std::cout << "Server start..." << std::endl;
    std::cout << "Listen port:" << port_ << std::endl;
    // 然后进行循环accept
    while (1)
    {
        struct sockaddr_in client_socketaddr{}; // 用来存放接收到的客户端地址信息
        socklen_t client_socketaddr_len = sizeof(client_socketaddr);
        Socket client_socket(accept(listen_socket_.get_socketfd(),
                                    (struct sockaddr *)&client_socketaddr,
                                    &client_socketaddr_len));
        if (client_socket.get_socketfd() < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("accept:");
            return 1;
        }
        //打印客户端地址
        char ip[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET,&client_socketaddr.sin_addr,ip,sizeof(ip)) == nullptr)
        {
            perror("inet_ntop:");
            continue;  //不处理这个，去处理其他客户端
        }
        std::cout << "Client connected: " << ip << ":" << ntohs(client_socketaddr.sin_port) << std::endl; 
        handleclient(client_socket.get_socketfd());
    }
    return 0;
}

void Tcpserver::handleclient(int fd)
{
    //循环接收信息
    char buf[4096]{};

    while (1)
    {
        ssize_t n = recv(fd,buf,sizeof(buf),0);
        if (n > 0)
        {
            std::cout.write(buf,n);
            std::cout.flush();
        }
        else if (n == 0)
        {
            std::cout << "Client disconnected" << std::endl;
            return ;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else
        {
            perror("recv:");
            return ;
        }
    }
    
}