//实现封装的tcpclient类

#include "network/Socket.h"
#include "network/Tcpclient.h"
#include <iostream>

//构造函数的实现
Tcpclient::Tcpclient(std::string ip,int port) 
: ip_(ip) , port_(port) , client_socket_(socket(AF_INET,SOCK_STREAM,0))
{}

//实现run函数
int Tcpclient::run()
{
    //先判定client_socket_是否初始化成功
    if (client_socket_.get_socketfd() < 0)
    {
        perror("socket:");
        return 1;
    }
    //然后进行connect连接
    //需要先绑定服务端地址
    struct in_addr addr{};
    if (inet_pton(AF_INET,ip_.data(),&addr) != 1)
    {
        perror("inet_pton:");
        return 1;
    }
    struct sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_);
    server_address.sin_addr = addr;
    int ret = connect(client_socket_.get_socketfd(),
                      (struct sockaddr *)&server_address,sizeof(server_address));
    if (ret < 0)
    {
        perror("connect:");
        return 1;
    }
    std::cout << "Connected to server:" << ip_ << ":" << port_ << std::endl;
    //连接好了，开始发信息
    std::string massege;
    while (1)
    {
        std::cout << "send massege:";
        if(!getline(std::cin,massege)) {break;}
        if (massege == "/quit") {break;}
        massege.push_back('\n'); //加上这个表示一句话结束
        if(sendall(massege) != 0) 
        {
            return 1;
        }
    }
    return 0;
}

int Tcpclient::sendall(const std::string& massege)
{
        //定义一个指针偏移
        size_t offset = 0;
        while (offset < massege.size())  //表明信息没发送完
        {
            ssize_t n = send(client_socket_.get_socketfd(),
                             massege.data() + offset,
                             massege.size() - offset , MSG_NOSIGNAL);
            if (n > 0)  //表示发送了n个字节信息
            {
                offset += static_cast<size_t>(n);
            }
            else if (errno == EINTR && n < 0)
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
        return 0;  
}