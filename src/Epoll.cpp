#include "network/Epoll.h"
#include <stdio.h>

//epoll类的初始化
Epoll::Epoll() : epoll_fd_(epoll_create1(EPOLL_CLOEXEC)) 
{ }

//初始化后检查是否成功
bool Epoll::valid() const noexcept  //只读，并且不会发生异常
{
    return epoll_fd_.get_socketfd() >= 0;  //小于0返回假
}
//增加监控事件
bool Epoll::add(int fd,uint32_t event)
{
    if(!valid())
    {
        return false;
    }
    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = event;
    if(epoll_ctl(epoll_fd_.get_socketfd(),EPOLL_CTL_ADD,fd,&ev) < 0)
    {
        perror("epoll_add:");
        return false;
    }
    return true;
}
//修改监控事情权限
bool Epoll::modify(int fd,uint32_t event)
{
    if(!valid())
    {
        return false;
    }
    struct epoll_event ev{};
    ev.data.fd = fd;
    ev.events = event;
    if(epoll_ctl(epoll_fd_.get_socketfd(),EPOLL_CTL_MOD,fd,&ev) < 0)
    {
        perror("epoll_mod:");
        return false;
    }
    return true;
}
//删除监控事件
bool Epoll::remove(int fd)
{
    if(!valid())
    {
        return false;
    }
    if(epoll_ctl(epoll_fd_.get_socketfd(),EPOLL_CTL_DEL,fd,nullptr) < 0)
    {
        perror("epoll_del:");
        return false;
    }
    return true;
}
//epoll开始等待
int Epoll::wait(epoll_event *events,int max_events,int timeout)
{
    return epoll_wait(epoll_fd_.get_socketfd(),events,max_events,timeout);
}