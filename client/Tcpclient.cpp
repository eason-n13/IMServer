// 实现封装的tcpclient类

#include "network/Socket.h"
#include "network/Tcpclient.h"
#include <iostream>
#include <poll.h>

// 构造函数的实现
Tcpclient::Tcpclient(std::string ip, int port)
    : ip_(ip), port_(port), client_socket_(socket(AF_INET, SOCK_STREAM, 0))
{
}

// 实现run函数
int Tcpclient::run()
{
    // 先判定client_socket_是否初始化成功
    if (client_socket_.get_socketfd() < 0)
    {
        perror("socket:");
        return 1;
    }
    // 然后进行connect连接
    // 需要先绑定服务端地址
    struct in_addr addr{};
    if (inet_pton(AF_INET, ip_.data(), &addr) != 1)
    {
        perror("inet_pton:");
        return 1;
    }
    struct sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port_);
    server_address.sin_addr = addr;
    int ret = connect(client_socket_.get_socketfd(),
                      (struct sockaddr *)&server_address, sizeof(server_address));
    if (ret < 0)
    {
        perror("connect:");
        return 1;
    }
    std::cout << "Connected to server:" << ip_ << ":" << port_ << std::endl;
    // 连接建立好了，建立poll来实现对客户端的监听,主要监听键盘输入与客户端输入
    struct pollfd pfds[2]{};
    pfds[0].fd = STDIN_FILENO;                  // 监听键盘有没有输入
    pfds[0].events = POLLIN;                    // 监听读事件
    pfds[1].fd = client_socket_.get_socketfd(); // 监听客户端有没有消息
    pfds[1].events = POLLIN;
    // 连接好了，开始发信息
    std::string massege;
    while (1)
    {
        // 使用poll进行监听
        ret = poll(pfds, 2, -1);   //如果没有事件会一直阻塞在这里
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("poll");
            return 1;
        }
        if (pfds[0].revents & (POLLNVAL))  //POLLNVAL这个是无效的fd
        {
            std::cerr << "stdin poll error\n";
            return 1;
        }
        //POLLIN | POLLHUP| POLLERR，这些信号recv会处理
        //键盘输入，也就是自己想要发送的信息
        if (pfds[0].revents & (POLLIN | POLLHUP| POLLERR)) // 代表pfds[0].fd的事件触发
        {
            constexpr size_t MAX_LINE = 64 * 1024;  //声明一个最大信息长度
            char buf[4096]{};
            // 不能使用getline，因为他要求读到‘\n’才结束，而poll只要检查到缓存区有内容就会唤醒
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n > 0)
            {
                massege.append(buf, n);
                while (true)
                {
                    // 存在一次发送多条消息的情况，如：hello\n byb\n hi\n
                    size_t pos = massege.find('\n'); // 所以要循环发送
                    if (pos == std::string::npos)
                    {
                        //没找到换行，判断要发送的信息是否长度超出
                        if (massege.size() > MAX_LINE)
                        {
                            std::cerr << "Message too long\n";
                            return 1;
                        }
                        break; // 如果没有就退出
                    }
                    if (pos > MAX_LINE)  //找到换行符的位置，发现太长
                    {
                        std::cerr << "Message too long\n";
                        return 1;
                    }
                    // 有就发送
                    std::string line = massege.substr(0, pos + 1); // 先截取
                    massege.erase(0, pos + 1);                     // 在删除
                    if (line == "/quit\n")
                    {
                        std::cout << "client exit!!!";
                        return 0;
                    }
                    // 然后发送
                    if (sendall(line) != 0)
                    {
                        return 1;
                    }
                }
            }
            else if (n == 0)  //对端关闭连接，断开
            {
                // 把缓存区的内容补上然后发送
                if (!massege.empty())
                {
                    massege.push_back('\n');
                    if (massege == "/quit\n")
                    {
                        std::cout << "client exit!!!";
                        return 0;
                    }
                    if (sendall(massege) != 0)
                    {
                        return 1;
                    }
                }
                return 0;
            }
            else
            {
                if (errno == EINTR)
                {
                    continue;
                }
                perror("read:");
                return 1;
            }
        }
        if (pfds[1].revents & POLLNVAL)
        {
            std::cerr << "server socket invalid\n";
            return 1;
        }
        //对面发送的信息
        if (pfds[1].revents & (POLLIN | POLLHUP | POLLERR)) // 代表pfds[1].fd的事件触发,客户端需要接收
        {

            // 接收信息,一次没接收完没事，下次poll还会继续
            char buf[4096]{};
            ssize_t n = recv(client_socket_.get_socketfd(), buf,
                             sizeof(buf), 0);
            if (n > 0)
            {
                std::cout.write(buf, n);
                std::cout.flush();
            }
            else if (n == 0)
            {
                std::cout << "server disconnected" << std::endl;
                return 0;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("recv:");
                return 1;
            }
        }
    }
    return 0;
}

//这里的massege是已经判断好的
int Tcpclient::sendall(const std::string &massege)
{
    // 定义一个指针偏移
    size_t offset = 0;
    while (offset < massege.size()) // 表明信息没发送完
    {
        ssize_t n = send(client_socket_.get_socketfd(),
                         massege.data() + offset,
                         massege.size() - offset, MSG_NOSIGNAL);
        if (n > 0) // 表示发送了n个字节信息
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