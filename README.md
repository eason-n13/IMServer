# Linux-IM-Server

Linux C++ 即时通信服务器学习项目。先完成 TCP 通信与 epoll Reactor，再逐步加入线程池、MySQL、Redis 和 HTTP/WebSocket。

## 当前进度

当前处于第 1 周第 1 天：工程构建与启动参数校验。

- CMake 生成 `IMServer`、`IMClient` 两个可执行程序。
- 使用 C++17，关闭编译器语言扩展，开启 `-Wall -Wextra -Wpedantic`。
- 服务端校验参数数量及端口范围；客户端额外校验 IPv4 地址。
- 参数合法返回 `0`，参数错误返回 `1`。

目前程序校验参数、打印信息后立即退出，尚未创建 socket，也不会监听端口或连接服务器。

## 环境要求

- Linux
- 支持 C++17 的编译器（如 GCC）
- CMake 3.16 或更高版本
- Make 或其他 CMake 支持的构建工具
- Git；后续调试使用 GDB

当前阶段不依赖 MySQL、Redis、OpenSSL 或 Boost，后续按模块引入。

## 构建

以下命令均在项目根目录执行。

Debug 版本，用于开发和 GDB 调试：

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j
```

Release 版本，用于后续性能测试：

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j
```

## 运行

```bash
./build/debug/IMServer 8888
./build/debug/IMClient 127.0.0.1 8888
```

服务端接受一个端口参数；客户端接受一个 IPv4 地址和一个端口参数。端口必须是完整的十进制数字字符串，取值范围为 `1–65535`。客户端暂不接受域名或 IPv6 地址。

运行 Release 版本时，将路径中的 `debug` 替换为 `release`。

## 手动验收

Debug 和 Release 两种构建都应检查以下场景：

| 场景 | 预期退出码 |
| --- | --- |
| 服务端端口 `8888`，客户端地址 `127.0.0.1`、端口 `8888` | 0 |
| 边界端口 `1`、`65535` | 0 |
| 缺少参数或多余参数 | 1 |
| 端口 `0`、`65536`、`-1`、`8888abc`、空字符串、超大整数 | 1 |
| 客户端地址 `999.1.1.1`、`abc`、空字符串 | 1 |

例如，执行错误参数测试后立即查看退出码：

```bash
./build/debug/IMServer 8888abc
echo $?
```

## 开发路线

| 周次 | 学习与实现内容 |
| --- | --- |
| 1 | 工程构建、参数校验、RAII Socket、基础 TCP 收发 |
| 2–3 | 非阻塞 epoll、Reactor 重构、连接生命周期与超时 |
| 4 | 自定义帧协议、拆包粘包、心跳、终端客户端 |
| 5 | pthread 线程池、有界任务队列、eventfd |
| 6–7 | MySQL 连接池、用户认证、群聊私聊、消息持久化 |
| 8–9 | Redis 会话与缓存、异步日志、优雅退出 |
| 10 | TCP 版回归测试、故障排查与压力测试 |
| 11–12 | HTTP/WebSocket 网关、浏览器测试客户端与最终交付 |

每阶段完成编码、测试、原理复述和 Git 提交后再推进。规划中的功能不代表已实现。

## 目录

```text
IMServer/
├── CMakeLists.txt
├── README.md
├── include/          # 后续模块头文件
├── src/main.cpp      # 服务端入口
├── client/main.cpp   # 客户端入口
└── build/            # 本地构建产物，不提交 Git
```

后续按需加入 `tests/`、`config/`、`docs/` 和 `third_party/`。运行数据与日志目录 `data/`、`logs/` 应在启用前加入 `.gitignore`。

## 许可证

Apache License 2.0，见 [LICENSE](LICENSE)。
