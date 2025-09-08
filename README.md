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



## v0.4-module4 **RESP Protocol Compatibilityl** 

todo: 使服务器兼容 Redis 的 RESP（REdis 序列化协议），以便它可以与 redis-cli 交互。

- 解析传入的客户端数据以提取命令和参数，处理多行格式和部分数据（由于非阻塞 I/O）。

- 根据 RESP 格式响应（例如，+OK\r\n，\$5\r\nvalue\r\n，$-1\r\n）


### 细节
class Command 进行了修改
- 修改了处理流程，接受一个 std::string_view 和一个 size_t& consumed 参数，允许部分缓冲区解析和跟踪已消耗的字节。

- 添加了 parseResp 函数来解析 RESP 数组，原先解析返回的 tokens 作为std::vector\<std::string_view> 引用参数传入。


class Server 进行了修改
- 更新了 handleClientEvent，使其使用客户端的缓冲区调用 Command::process，并跟踪已消耗的字节以处理部分 RESP 消息

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
```bash
redis-cli -h 127.0.0.1 -p 6379
SET mykey hello
OK
GET mykey
"hello"
GET unknown
(nil)

Comm
(error) ERR unknown command 'Comm'
SET mykey
(error) ERR wrong number of arguments for 'SET' command
```


## v0.5-module5 **AOF Persistence** 
todo: 添加仅追加文件（AOF）持久化功能，以确保数据在服务器重启时能够持久保存
- 将SET命令立即附加到文件

- 启动时，服务器读取 AOF 文件并重新执行命令以重建内存存储。 （读取整个文件为连续 buffer，然后逐步解析 RESP 序列）

### 细节
class Store 进行了修改
- 添加了 aof_file_ 来存储 AOF 文件路径、 aof_ ( std::ofstream ) 用于日志记录，以及方法 logCommand 和 replayAof
    - logCommand 将命令写入 RESP 数组并刷新到磁盘
    - replayAof 读取 AOF 文件，使用 Command::process 解析 RESP 命令，并重建存储。

class Server 进行了修改 
- 修改构造函数以接受用于 Store 初始化的 AOF 文件路径

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
```bash
redis-cli -h 127.0.0.1 -p 6379
SET key1 value1
OK
SET key2 value2
OK

====== 重启 server 后 ======

redis-cli -h 127.0.0.1 -p 6379
GET key1
"value1"
GET key2
"value2"
```

## v0.6-module6 **Key Expiration(TTL)** 
todo: 为自动过期键添加对生存时间（TTL）的支持。

- 添加 EXPIRE 命令来为密钥设置 TTL，并与模块 4 命令注册表和模块 5 AOF 日志记录集成。

### 细节
class Store 进行了修改
- 添加了 expirations_ map 来存储密钥过期时间戳。
- 添加了 setExpire 来设置 TTL 和 cleanupExpiredKeys 以进行定期清理。
- 添加了 cleanupExpiredKeys 以从 data_ 和 expirations_ 中删除过期的密钥。


class Command 进行了修改

- 添加了 expireCommand 来处理 EXPIRE 命令，解析 TTL 并调用 store.setExpire


class Server进行了修改
- 添加了 last_cleanup_ 和 CLEANUP_INTERVAL 以进行定期清理


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
```bash
redis-cli -h 127.0.0.1 -p 6379
SET key1 value1
EXPIRE key1 5
GET key1  # Before 5 seconds
"value1"
# Wait 6 seconds
GET key1
(nil)  # Lazy deletion

========== 测试持久性 ===========

SET key2 value2
EXPIRE key2 3600
# Stop and restart server

redis-cli -h 127.0.0.1 -p 6379
GET key2
"value2"  # Should persist with TTL
```

## v0.7-module7 **Atomic Transactions**
todo: 支持 Redis 风格的原子命令执行事务。
- 实现 MULTI、EXEC 和 DISCARD 命令。
- 确保分组命令的原子性。

### 细节
将 Client 结构体从 Server 中移出，放入单独的头文件，防止后续循环依赖，新增事务状态、事务队列


class Server 进行了修改
- 向客户端结构添加事务状态和命令队列，修改 handleClientEvent 以在事务期间对命令进行排队。


Class Command 进行了修改
- 添加 MULTI 、 EXEC 和 DISCARD 命令，更新流程以处理事务排队。

class Store 进行了修改
- 修改 Command::process 的调用


### 目录结构
    mini-redis
    |-- include/
        |-- server.hpp
        |-- client.hpp
        |-- store.hpp
        |-- command.hpp
    |-- src/
        |--server.cp
        |-- store.cpp
        |-- command.cpp
        |-- main.cpp
    |-- CMakeLists.txt


### 测试
```bash
redis-cli -h 127.0.0.1 -p 6379
MULTI
OK
SET key1 value1
QUEUED
EXPIRE key1 10
QUEUED
GET key1
QUEUED
EXEC
1) OK
2) (integer) 1
3) "value1"
DISCARD  # Without MULTI
(error) ERR DISCARD without MULTI
MULTI
OK
SET key2 value2
QUEUED
DISCARD
OK
GET key2
(nil)
```

## v0.8-module8 **Command Management System**
todo: 将命令组织成一个模块化、可扩展的系统。


### 细节
class Command 进行了修改
- 添加了具有纯虚拟执行方法的抽象 Command 类。
- 引入 CommandFactory 单例来管理命令注册和创建
- 更新 Command::process 以使用工厂并分派到命令对象。

### 目录结构
    mini-redis
    |-- include/
        |-- server.hpp
        |-- client.hpp
        |-- store.hpp
        |-- command.hpp
    |-- src/
        |--server.cp
        |-- store.cpp
        |-- command.cpp
        |-- main.cpp
    |-- CMakeLists.txt


## v0.9-module9 **Buffer Management**
todo: 处理 TCP 的粘包问题。

### 细节
添加了一个带有固定尺寸缓冲区的新的Ringbuffer类（有待测试）

### 目录结构
    mini-redis
    |-- include/
        |-- server.hpp
        |-- client.hpp
        |-- store.hpp
        |-- command.hpp
        |-- ring_buffer.hpp
    |-- src/
        |--server.cp
        |-- store.cpp
        |-- command.cpp
        |-- ring_buffer.cpp
        |-- main.cpp
    |-- CMakeLists.txt
