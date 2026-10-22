#pragma once

#include "network/Socket.h"
#include <sys/epoll.h>


class Epoll
{
private:
    Socket epoll_fd_;
public:
    Epoll();
    bool valid() const noexcept;
    bool add(int fd, uint32_t events);
    bool modify(int fd, uint32_t events);
    bool remove(int fd);

    int wait(epoll_event *events,int max_events,int timeout);
};