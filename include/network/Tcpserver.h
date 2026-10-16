//声明服务器类的结构与接口
#pragma once

#include "network/Socket.h"
#include <sys/socket.h>

class Tcpserver
{
private:
    int port_;              //端口号
    Socket listen_socket_;  //监听套接字
    void handleclient(int fd);     //接收客户端信息
public:
    explicit Tcpserver(int port);
    ~Tcpserver(){};
    int run();
};
