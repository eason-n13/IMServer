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
    Epoll epoll_socket;
    if (!epoll_socket.valid())
    {
        perror("epoll_create:");
        return 1;
    }
    // 把这个监听客户端加入epoll中进行监听
    if(!epoll_socket.add(listen_socket_.get_socketfd(),EPOLLIN))
    {
        return 1;
    }
    // 定义一个存放就绪事件的数组
    struct epoll_event events[10];
    // 然后进行循环accept
    while (1)
    {
        // 阻塞等待，等待内核说有任务就绪后就继续进行,会返回n个就绪好的任务
        int n = epoll_socket.wait(events,10,-1);
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
                        // 非阻塞模式读完了返回-1（EAGAIN 或者 EWOULDBLOCK），不会死等
                        if (errno == EAGAIN || errno == EWOULDBLOCK) // 没有连接了
                        {
                            break;
                        }

                        perror("accept");
                        break;
                    }
                    else // client_fd > 0，接受到用户端的连接
                    {
                        // 在堆上创建一个client_socket的副本对象，然后这个智能指针指向它
                        clientstate client{};
                        client.socket = std::make_unique<Socket>(client_fd);
                        // 对这些客户端fd进行设置无阻塞
                        if (!setnonblocking(client_fd))
                        {
                            continue;
                        }
                        // 将这个客户端加入到epoll监听中
                        if(!epoll_socket.add(client_fd,EPOLLIN))
                        {
                            return 1;
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
                if (it == clients_.end()) // 客户端连接不存在
                {
                    continue;
                }
                // 事件类型
                uint32_t event_type = events[i].events;
                // 需要接收客户端发送的消息
                if (event_type & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR))
                {
                    bool alive = handleclient(client_fd, epoll_socket);
                    if (!alive) // handleclient处理失败，对面客户端断开或者失效
                    {
                        removeclient(client_fd, epoll_socket);
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
                        removeclient(client_fd, epoll_socket);
                        continue;
                    }
                    // 结束循环后并发送成功后判断信息发送完了吗
                    if (client.sent == client.output.size())
                    {
                        //发送完了回收权限
                        if (!epoll_socket.modify(client_fd,EPOLLIN | EPOLLRDHUP))
                        {
                            // 在修改失败的话清理掉epoll中的这个连接
                            removeclient(client_fd, epoll_socket);
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

bool Tcpserver::handleclient(int fd, Epoll& epoll_fd)
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
                if(!processmessage(fd, message, epoll_fd))
                {
                    return false;
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

// 用户名函数
int Tcpserver::handlename(int fd, const std::string &username,Epoll& epoll_fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return 1;
    }
    // 1. 当前客户端是否已经命名？
    if (!it->second.username.empty())
    {
        if(!enqueue_message(fd, "当前连接已设置用户名\n", epoll_fd))
        {
            return -1;
        }
        return 1;
    }
    // 2. 用户名长度是否合法？用户名长度为 1–32 字节。
    if (username.size() < 1 || username.size() > 32)
    {
        if(!enqueue_message(fd, "用户名长度不对\n", epoll_fd))
        {
            return -1;
        }
        return 1;
    }
    // 3. 用户名字符是否合法？只允许 ASCII 字母、数字、下划线
    if (std::any_of(username.begin(), username.end(), [](unsigned char c)
                    {
                        return !(std::isalnum(c) || c == '_'); // 都满足就不执行下面的，返回假
                    }))
    { // 为真执行下面的
        if(!enqueue_message(fd, "用户名字符非法\n", epoll_fd))
        {
            return -1;
        }
        return 1;
    }
    // 4. 用户名是否已经被占用？
    if (online_users_.find(username) != online_users_.end()) // 存在这个名字
    {
        if(!enqueue_message(fd, "用户名已经被占用\n", epoll_fd))
        {
            return -1;
        }
        return 1;
    }
    // 5. 全部通过后更新两个容器
    it->second.username = username;
    online_users_.emplace(username, fd);
    if(!enqueue_message(fd, "用户名设置成功：" + username + "\n", epoll_fd))
    {
        return -1;
    }
    return 0;
}

// 判断命名
bool Tcpserver::processmessage(int fd, const std::string &message, Epoll& epoll_fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return false;
    }
    // 1. 去掉末尾的 \n
    std::string line = message;
    if (!line.empty() && line.back() == '\n')
    {
        line.pop_back();
    }
    // 2. 判断 /name 命令,允许未命名客户端执行
    if (line.rfind("/name ", 0) == 0 || line == "/name") // 检查是不是\name开头
    {
        if (line == "/name")
        {
            return enqueue_message(fd, "用户名不能为空\n", epoll_fd);
        }
        if(handlename(fd, line.substr(6),epoll_fd) == -1)
        {
            return false;
        }
        return true;
    }
    if (it->second.username.empty())
    {
        return enqueue_message(fd, "请先设置用户名\n", epoll_fd);
    }
    if (line == "/msg" || line.rfind("/msg ", 0) == 0)
    {
        if (line == "/msg")
        {
            return enqueue_message(fd, "发送信息不能为空\n", epoll_fd);
        }
        if(!handleprivatemessage(fd, line.substr(5), epoll_fd))
        {
            return false;
        }
        return true;
    }
    // 其他未知命令
    if (!line.empty() && line[0] == '/')
    {
        return enqueue_message(fd, "未知命令\n", epoll_fd);
    }
    // 有用户名,并且不是未知命令就可以进行广播
    // 构造发送文本
    std::string text = "[" + it->second.username + "]:" + message;
    std::vector<int> remove_fds; // 保存超限文件
    // 进行广播，讲消息加入到格客户端的缓冲区，便于他们提取
    for (auto &it : clients_)
    {
        if (it.first == fd) // 自己的输出缓存区不保存广播内容
        {
            continue;
        }
        if (!enqueue_message(it.first, text, epoll_fd))
        {
            remove_fds.push_back(it.first);
        }
    }
    for (int target_fd : remove_fds) // 将超限的删掉
    {
        removeclient(target_fd, epoll_fd);
    }
    return true;
}

// 消息队列，函数作用就是，将text放到fd的output区域，并开启fd的接受功能
bool Tcpserver::enqueue_message(int fd, const std::string &text, Epoll& epoll_fd)
{
    constexpr size_t MAX_OUTPUT = 1024 * 1024;
    // 1. 查找目标客户端
    // 使用 clients_.find(fd)
    // 不存在则返回 false
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return false;
    }
    // 2. 获取目标客户端的 clientstate
    // 使用引用，避免复制
    clientstate &client = it->second;
    // 3. 计算当前尚未发送的数据量
    // pending = output.size() - sent
    size_t pending = client.output.size() - client.sent;
    // 4. 判断追加 text 后是否超过 1 MiB
    // 超限返回 false
    if (text.size() > MAX_OUTPUT || text.size() > MAX_OUTPUT - pending)
    {
        return false;
    }
    // 5. 将 text 追加到 output
    // 注意：使用 append()，不能使用 =
    if (client.sent > 0)
    {
        client.output.erase(0, client.sent);
        client.sent = 0;
    }
    client.output.append(text);
    // 6. 开启该客户端的 EPOLLOUT
    if(!epoll_fd.modify(fd,EPOLLIN | EPOLLRDHUP | EPOLLOUT))
    {
        return false;
    }
    // 7. 入队成功
    return true;
}

void Tcpserver::removeclient(int fd, Epoll& epoll_fd)
{
    // 1. 查找客户端
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return;
    }
    // 2. 如果已经设置用户名
    // 从 online_users_ 删除用户名
    if (!it->second.username.empty())
    {
        online_users_.erase(it->second.username);
    }
    // 3. 从 epoll 中移除客户端
    // EPOLL_CTL_DEL
    epoll_fd.remove(fd);
    // 4. 从 clients_ 中删除客户端
    // unique_ptr<Socket> 会自动释放 Socket
    clients_.erase(fd);
}

bool Tcpserver::handleprivatemessage(int fd, const std::string &args, Epoll& epoll_fd)
{
    // 1. 根据 fd 查找发送者
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return false;
    }
    // 2. 从 args 中提取目标用户名和消息正文
    // 例如 "Bob hello there"
    // target = "Bob"
    // content = "hello there"
    size_t pos = args.find(' ');
    if (pos == std::string::npos)
    {
        return enqueue_message(fd, "私聊格式错误，正确格式：/msg 用户名 消息\n", epoll_fd);
    }
    std::string target = args.substr(0, pos);
    std::string content = args.substr(pos + 1);
    // 3. 判断格式是否正确
    // 目标用户名或消息正文不能为空
    if (target.empty() || content.empty())
    {
        
        return enqueue_message(fd, "私聊格式错误：用户名或消息不能为空\n", epoll_fd);
    }
    // 4. 判断是否给自己发私聊
    if (target == it->second.username)
    {
        
        return enqueue_message(fd, "不能给自己发送私聊\n", epoll_fd);
    }
    // 5. 在 online_users_ 中查找目标用户
    // 找不到则提示用户不在线
    if (online_users_.find(target) == online_users_.end())
    {
        
        return enqueue_message(fd, "用户不在线\n", epoll_fd);
    }
    // 6. 构造私聊消息
    // [private Alice]: hello there
    std::string text = "[private " + it->second.username + "]:" + content + '\n';
    // 7. 使用 enqueue_message() 给目标用户入队
    int target_fd = online_users_[target];
    if (!enqueue_message(target_fd, text, epoll_fd))
    {
        removeclient(target_fd, epoll_fd);
        return enqueue_message(fd, "私聊发送失败：目标连接不可用\n", epoll_fd);
    }
    // 8. 入队成功后，给发送者返回确认提示
    // 注意：入队成功不代表 Bob 已经收到消息
    return enqueue_message(fd, "私聊消息已入队\n", epoll_fd);
}