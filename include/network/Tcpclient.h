//客户端服务的封装
#pragma once

#include "network/Socket.h"
#include <sys/socket.h>
#include <string>
#include <arpa/inet.h>

class Tcpclient
{ 
private:
    std::string ip_; //地址
    int port_; //端口
    Socket client_socket_;
    int sendall(const std::string& maseege);
public:
    Tcpclient(std::string ip_,int port_);
    int run();
};
