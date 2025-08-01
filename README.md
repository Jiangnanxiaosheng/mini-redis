## v0.1-module1 **Basic Socket Server**


todo: 建立一个最小的 TCP 服务器，接受客户端连接并处理基本请求。

- 使用 std::unordered_map 在内存中存储键值对

- 使用基于文件的简单协议（GET key、SET key value）

### 细节
- class Server 封装了套接字设置和连接处理，遵循 RAII 进行资源管理

- class Store 分离了数据存储逻辑，便于后续添加过期或持久化等功能

- class Command 隔离命令解析，便于扩展

### 目录结构
    mini-redis
    |-- include/
        |-- server.hpp
        |-- store.hpp
        |-- command.hpp
    |-- src/
        |--server.cp
        |-- store.cpp
        |-- command.cpp
        |-- main.cpp
    |-- CMakeLists.txt


### 测试
Test with telnet
```
telnet 127.0.0.1 6379
SET mykey hello
+OK
GET mykey
+hello
(按下 ctrl+] 组合，再输入 `quit` 退出)
```


## v0.2-module2 **Basic Command Parser**


todo: 解析简单的客户端命令以支持基本的键值操作
- 解析器必须可靠地处理多个命令（SET、GET 等），验证输入，并生成类似 Redis 的响应
- 它应该能够扩展以支持额外的命令（例如，DEL、EXPIRE），而无需进行重大重构
- 使用命令模式，通过注册handler的方式实现可扩展的命令处理

### 细节
class Command 进行了修改
- 添加了使用 std::function 定义命令处理器的 Handler 类型

- 引入了 handlers_ 作为静态 std::unordered_map 来存储命令名到处理器映射

- 添加了 registerCommand 来动态注册命令

- 使用 std::string_view 以实现高效的字符串处理


### 目录结构
    mini-redis
    |-- include/
        |-- server.hpp
        |-- store.hpp
        |-- command.hpp
    |-- src/
        |--server.cp
        |-- store.cpp
        |-- command.cpp
        |-- main.cpp
    |-- CMakeLists.txt


## v0.3-module3 **Non-Blocking I/O and epoll**
todo: 将阻塞服务器转换为使用 epoll 的高性能非阻塞服务器

- 事件循环会持续调用 epoll_wait()来检查事件，然后将它们分派给适当的处理器（例如，接受新连接、读取客户端数据）。


### 细节
class Server 进行了修改
- 添加 epoll 实例、客户端状态跟踪和事件循环方法

- 引入了 Client 结构体用于存储每个客户端的状态：接收数据的缓冲区、发送数据的响应以及 has_pending_write 用于跟踪写事件

- 添加了 clients_ map 来跟踪所有通过文件描述符连接的客户端
- handleNewConnection
    - 在循环中接受多个连接，将它们设置为非阻塞，并添加到 epoll 中，使用 EPOLLIN | EPOLLHUP | EPOLLERR。
- handleClientEvent
    - 将数据读入 client.buffer，处理以\r\n 分隔的完整命令
    - 将响应累积在 client.response 中
    - 当有响应待处理时动态添加/删除 EPOLLOUT
    - 在发生错误或挂断时关闭客户端

### 目录结构
    mini-redis
    |-- include/
        |-- server.hpp
        |-- store.hpp
        |-- command.hpp
    |-- src/
        |--server.cp
        |-- store.cpp
        |-- command.cpp
        |-- main.cpp
    |-- CMakeLists.txt
