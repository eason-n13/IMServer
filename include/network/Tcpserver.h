//声明服务器类的结构与接口
#pragma once

#include "network/Socket.h"
#include <sys/socket.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <cctype>
#include <algorithm>

struct clientstate
{
    std::unique_ptr<Socket> socket;
    std::string input;
    std::string output;
    size_t sent = 0;   //偏移位

    std::string username;  //保存用户名
};


class Tcpserver
{
private:
    int port_;              //端口号
    Socket listen_socket_;  //监听套接字
    std::unordered_map<int,clientstate> clients_;//用于保护客户端fd，防止被提前析构
    std::unordered_map<std::string,int> online_users_;  //保存在线用户与其fd
    bool handleclient(int fd,int epoll_fd);     //接收客户端信息
    bool setnonblocking(int fd);   //设置无阻塞模式,当没信息返回-1，以便处理其他进程
    int handlename(int fd, const std::string &username,int epoll_fd); //封装处理用户名函数
    bool processmessage(int fd, const std::string &message, int epoll_fd); //用于处理客户是进行命名还是聊天
    bool enqueue_message(int fd, const std::string &text, int epoll_fd);
    void removeclient(int fd, int epoll_fd); //删除客户端连接
    bool handleprivatemessage(int fd,const std::string &args,int epoll_fd); //私发消息函数
public:
    explicit Tcpserver(int port);
    ~Tcpserver(){};
    int run();
};
