#include "server.hpp"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

Server::Server(int port, const std::string& aof_file) : store_(aof_file) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to set socket options");
    }

    setNonBlocking(server_fd_);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Bind failed");
    }

    if (listen(server_fd_, 10) < 0) {
        close(server_fd_);
        throw std::runtime_error("Listen failed");
    }

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to create epoll instance");
    }

    // 将服务器套接字添加到epoll监听
    epoll_event ev;
    ev.data.fd = server_fd_;
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev) < 0) {
        close(epoll_fd_);
        close(server_fd_);
        throw std::runtime_error("Failed to add server socket to epoll");
    }
}

Server::~Server() {
    close(epoll_fd_);
    close(server_fd_);
    // 关闭所有客户端连接
    for (auto& [fd, _] : clients_) {
        close(fd);
    }
}

void Server::run() {
    epoll_event events[MAX_EVENTS];
    while (true) {
        // 采用带超时时间的 epoll_wait，定期检查过期键，防止长时间阻塞
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == server_fd_) {
                handleNewConnection();  // 处理新连接
            } else {
                handleClientEvent(fd, events[i].events);  // 处理客户端事件
            }
        }

        // 定期清理过期键
        auto now = std::chrono::system_clock::now();
        if (now - last_cleanup_ >= CLEANUP_INTERVAL) {
            store_.cleanupExpiredKeys();
            last_cleanup_ = now;
        }
    }
}

void Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("Failed to get socket flags");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("Failed to set non-blocking");
    }
}

void Server::handleNewConnection() {
    while (true) {
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 没有更多待处理连接
            }
            std::cerr << "Accept failed\n";
            return;
        }

        setNonBlocking(client_fd);

        epoll_event ev;
        ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;  // 监听可读、挂起和错误事件
        ev.data.fd = client_fd;

        // 将客户端套接字添加到epoll
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            close(client_fd);
            std::cerr << "Failed to add client to epoll\n";
            return;
        }

        // 将新客户端添加到客户端映射表
        clients_.emplace(client_fd, Client{});
    }
}

void Server::handleClientEvent(int client_fd, uint32_t events) {
    auto& client = clients_.at(client_fd);

    // 处理连接挂起或错误事件
    if (events & (EPOLLHUP | EPOLLERR)) {
        closeClient(client_fd);
        return;
    }

    // 处理可读事件（客户端发送数据）
    if (events & EPOLLIN) {
        char buffer[1024];  // 接收缓冲区
        while (true) {
            ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // 没有更多数据可读
                }
                closeClient(client_fd);
                return;
            }
            if (bytes_read == 0) {  // 客户端关闭连接
                closeClient(client_fd);
                return;
            }
            buffer[bytes_read] = '\0';
            client.buffer += buffer;

            // 处理 RESP 信息
            size_t consumed = 0;
            while (consumed < client.buffer.size()) {
                size_t bytes_consumed = 0;
                std::string response =
                    Command::process(client.buffer, bytes_consumed, store_, client);
                if (bytes_consumed == 0) {
                    break;
                }
                client.buffer.erase(0, bytes_consumed);
                client.response += response;
                consumed += bytes_consumed;
            }
        }
    }

    // 处理可写事件（发送响应给客户端）
    if (!client.response.empty() && !client.has_pending_write) {
        // 添加EPOLLOUT事件监听
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR;
        ev.data.fd = client_fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev) < 0) {
            closeClient(client_fd);
            return;
        }
        client.has_pending_write = true;  // 添加EPOLLOUT事件监听
    }

    // 发送响应数据
    if (events & EPOLLOUT && !client.response.empty()) {
        ssize_t bytes_sent = send(client_fd, client.response.c_str(), client.response.size(), 0);
        if (bytes_sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;  // 稍后重试
            }
            closeClient(client_fd);
            return;
        }

        // 移除已发送的数据
        client.response.erase(0, bytes_sent);
        if (client.response.empty()) {
            // 如果所有数据都已发送，取消EPOLLOUT监听
            epoll_event ev;
            ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
            ev.data.fd = client_fd;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev) < 0) {
                closeClient(client_fd);
                return;
            }
            client.has_pending_write = false;
        }
    }
}

void Server::closeClient(int client_fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);

    // 从客户端映射表中移除
    clients_.erase(client_fd);
}
