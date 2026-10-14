#pragma once  //防止头文件重复展开
#include <unistd.h>  //这个头文件中有linux的close


class Socket   //定义RAll socket,防止socket没有进行及时销毁
{
private:
    int fd_; //socket的文件描述符
public:
    //构造函数--注意对于socket要禁止拷贝，防止两个socket使用同一个文件描述符
    //还要禁止int的隐式装换
    explicit Socket(int fd = -1) noexcept : fd_(fd)//默认为-1
    { }
    ~Socket()
    {
        if (fd_ >= 0) //如果fd大于等于0，就说明还有套接字要关闭
        {
            close(fd_);
        }
    }
    //设置禁止拷贝,另外socket的文件描述符只能对其进行引用，不能修改，所以要加上const
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    //设计一个函数可以获得socket的文件描述符
    int get_socketfd() const noexcept  //只返回文件描述符，不修改socket对象，不抛出异常
    {
        return fd_;
    }
};
