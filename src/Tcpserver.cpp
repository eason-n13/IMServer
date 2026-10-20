#include "network/Tcpserver.h"

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <vector>

// 在这个函数实现Tcpserver.h的各个功能
// 首先实现构造函数
Tcpserver::Tcpserver(int port) : port_(port), listen_socket_(socket(AF_INET, SOCK_STREAM, 0)) // 传入端口和创建监听套接字
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
    // 成功后将listen_socket_设置为非阻塞
    if (!setnonblocking(listen_socket_.get_socketfd()))
    {
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
    // 创建epoll来管理
    Socket epoll_socket(epoll_create1(EPOLL_CLOEXEC)); // 创建一个epoll管理
    if (epoll_socket.get_socketfd() < 0)
    {
        perror("epoll_create1:");
        return 1;
    }
    // 创建服务端事件，加入刚刚创建的epoll中进行监控
    struct epoll_event ev{};
    ev.data.fd = listen_socket_.get_socketfd();
    ev.events = EPOLLIN;
    // 开始监听,这个epoll要同时监控服务端与各个客户端
    ret = epoll_ctl(epoll_socket.get_socketfd(), EPOLL_CTL_ADD,
                    ev.data.fd, &ev);
    if (ret < 0)
    {
        perror("epoll_ctl:");
        return 1;
    }
    // 定义一个存放就绪事件的数组
    struct epoll_event events[10];
    // 然后进行循环accept
    while (1)
    {
        // 阻塞等待，等待内核说有任务就绪后就继续进行,会返回n个就绪好的任务
        int n = epoll_wait(epoll_socket.get_socketfd(), events, 10, -1);
        if (n < 0)
        {
            if (errno == EINTR) // 被干扰中断
            {
                continue;
            }
            perror("epoll_wait");
            return 1;
        }
        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == listen_socket_.get_socketfd()) // 表示有新的客户端要连接
            {
                // 同时有多个客户端请求连接，所以需要进行循环，ET模式，需要一次取干净
                while (true)
                {
                    struct sockaddr_in client_socketaddr{}; // 用来存放接收到的客户端地址信息
                    socklen_t client_socketaddr_len = sizeof(client_socketaddr);
                    int client_fd = accept(listen_socket_.get_socketfd(),
                                           (struct sockaddr *)&client_socketaddr,
                                           &client_socketaddr_len);
                    if (client_fd < 0)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK) // 没有连接了
                        {
                            break;
                        }

                        perror("accept");
                        break;
                    }
                    else
                    {
                        // 在堆上创建一个client_socket的副本对象，然后这个智能指针指向它
                        clientstate client;
                        client.socket = std::make_unique<Socket>(client_fd);
                        // 对这些客户端fd进行设置无阻塞
                        if (!setnonblocking(client_fd))
                        {
                            continue;
                        }
                        // 将这个客户端加入到epoll监听中
                        struct epoll_event client_event{};
                        client_event.data.fd = client_fd;
                        // EPOLLRDHUP检查对端是否关闭连接
                        client_event.events = EPOLLIN | EPOLLRDHUP;
                        ret = epoll_ctl(epoll_socket.get_socketfd(), EPOLL_CTL_ADD, client_event.data.fd, &client_event);
                        if (ret < 0)
                        {
                            perror("epoll_ctl:");
                            continue;
                        }
                        // 转移所有权给类中对象，这样就不会在这个if后被析构
                        clients_.emplace(client_fd, std::move(client));
                    }
                }
            }
            else // 就是要处理客户端操作,现在有两种操作了，发送信息和接收信息了
            {
                int client_fd = events[i].data.fd;
                // 检查客户端是否存在
                auto it = clients_.find(client_fd);
                if (it == clients_.end())
                {
                    continue;
                }
                // 事件类型
                uint32_t event_type = events[i].events;
                if (event_type & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR)) // 需要接收消息
                {
                    bool alive = handleclient(client_fd, epoll_socket.get_socketfd());
                    if (!alive)
                    {
                        // 第一步：从 epoll 移除
                        if (epoll_ctl(
                                epoll_socket.get_socketfd(),
                                EPOLL_CTL_DEL,
                                client_fd,
                                nullptr) < 0)
                        {
                            perror("epoll_ctl DEL client");
                        }
                        // 第二步：从容器删除
                        clients_.erase(it);
                        // 删除后退出
                        continue;
                    }
                }
                if (event_type & EPOLLOUT) // 需要发送消息
                {
                    clientstate &client = it->second;
                    bool send_failed = false;
                    while (client.sent < client.output.size())
                    {
                        ssize_t n = send(client_fd,
                                         client.output.data() + client.sent,
                                         client.output.size() - client.sent,
                                         MSG_NOSIGNAL);
                        if (n > 0)
                        {
                            client.sent += static_cast<size_t>(n);
                        }
                        else if (n < 0 && errno == EINTR)
                        {
                            continue;
                        }
                        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                        {
                            // 没有信息要发送
                            break;
                        }
                        else
                        {
                            perror("send:");
                            send_failed = true;
                            break;
                        }
                    }
                    // 判断是否发送失败
                    if (send_failed)
                    {
                        // 第一步：从 epoll 移除
                        if (epoll_ctl(
                                epoll_socket.get_socketfd(),
                                EPOLL_CTL_DEL,
                                client_fd,
                                nullptr) < 0)
                        {
                            perror("epoll_ctl DEL client");
                        }
                        // 第二步：从容器删除
                        clients_.erase(it);
                    }
                    // 结束循环后判断信息发送完了吗
                    else if (client.sent == client.output.size())
                    {
                        epoll_event ev{};
                        ev.data.fd = client_fd;
                        ev.events = EPOLLIN | EPOLLRDHUP;
                        if (epoll_ctl(
                                epoll_socket.get_socketfd(),
                                EPOLL_CTL_MOD,
                                client_fd,
                                &ev) < 0)
                        {
                            perror("epoll_ctl MOD");
                            // 在修改失败的话清理掉epoll中的这个连接
                            epoll_ctl(epoll_socket.get_socketfd(),
                                      EPOLL_CTL_DEL,
                                      it->first, nullptr);
                            clients_.erase(it);
                            continue;
                        }
                        client.sent = 0;
                        client.output.clear();
                    }
                }
            }
        }
    }
}

bool Tcpserver::handleclient(int fd, int epoll_fd)
{
    // 循环接收信息  //现在升级，接收到消息后拼接然后发送给各个客户端
    char buf[4096]{};
    while (1)
    {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0)
        {
            clients_[fd].input.append(buf, n);
            while (true)
            {
                constexpr size_t MAX_LINE = 64 * 1024; // 定义一个64kb大小的长度
                size_t pos = clients_[fd].input.find('\n');
                if (pos == std::string::npos)
                {
                    if (clients_[fd].input.size() > MAX_LINE) // 没找到换行，但是长度超限
                    {
                        std::cerr << "Message too long\n";
                        return false;
                    }
                    break;
                }
                if (pos > MAX_LINE) // 找到换行，但是长度超限
                {
                    std::cerr << "Message too long\n";
                    return false;
                }
                std::string message = clients_[fd].input.substr(0, pos + 1);
                clients_[fd].input.erase(0, pos + 1);
                std::cout << "receive:" << message;
                // 现在发送给除了自己以外的客户端
                // 构造发送文本
                std::string text = "[fd=" + std::to_string(fd) + "]:" + message;
                std::vector<int> remove_fds; // 保存超限文件
                for (auto &it : clients_)
                {
                    constexpr size_t MAX_OUTPUT = 1024 * 1024; // 消息上限1mb
                    if (it.first == fd)
                    {
                        continue;
                    }
                    size_t pending = it.second.output.size() - it.second.sent; // 目前消息长度
                    if (text.size() > MAX_OUTPUT || pending > MAX_OUTPUT - text.size())
                    {
                        std::cerr << "Client output too large: " << it.first << '\n';
                        remove_fds.push_back(it.first);
                        continue;
                    }
                    else
                    {
                        it.second.output.append(text);
                    }
                    epoll_event ev{};
                    ev.data.fd = it.first;
                    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLOUT;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, it.first, &ev) < 0)
                    {
                        perror("epoll_ctl MOD EPOLLOUT");
                        //需要删除该连接
                        remove_fds.push_back(it.first);
                    }
                }
                for (int target_fd : remove_fds)  //将超限的删掉
                {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, target_fd, nullptr);
                    clients_.erase(target_fd);
                }
            }
        }
        else if (n == 0)
        {
            std::cout << "Client disconnected" << std::endl;
            return false;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) // et模式没数据返回-1，errno为
        {
            return true; // 当前没有更多数据，返回 epoll 事件循环
        }
        else
        {
            perror("recv:");
            return false;
        }
    }
}

bool Tcpserver::setnonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0); // 得到文件当前状态标志
    if (flags < 0)
    {
        perror("F_GETFL:");
        return false;
    }
    // 设置文件标志位，与无阻塞进行与操作，防止覆盖之前的文件标志位
    int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (ret < 0)
    {
        perror("F_SETFL:");
        return false;
    }
    return true;
}