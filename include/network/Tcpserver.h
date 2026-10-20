//声明服务器类的结构与接口
#pragma once

#include "network/Socket.h"
#include <sys/socket.h>
#include <unordered_map>
#include <memory>
#include <string>

struct clientstate
{
    std::unique_ptr<Socket> socket;
    std::string input;
    std::string output;
    size_t sent = 0;   //偏移位
};


class Tcpserver
{
private:
    int port_;              //端口号
    Socket listen_socket_;  //监听套接字
    std::unordered_map<int,clientstate> clients_;//用于保护客户端fd，防止被提前析构
    bool handleclient(int fd,int epoll_fd);     //接收客户端信息
    bool setnonblocking(int fd);   //设置无阻塞模式,当没信息返回-1，以便处理其他进程
public:
    explicit Tcpserver(int port);
    ~Tcpserver(){};
    int run();
};
