#pragma once
#include <sys/epoll.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "command.hpp"
#include "store.hpp"

class Server {
public:
    using time_point = std::chrono::system_clock::time_point;

    Server(int port, const std::string& aof_file);
    ~Server();
    void run();

private:
    struct Client {
        std::string buffer;
        std::string response;
        bool has_pending_write{false};
    };

    void setNonBlocking(int fd);
    void handleNewConnection();
    void handleClientEvent(int client_fd, uint32_t events);
    void closeClient(int client_fd);

    int server_fd_;
    int epoll_fd_;

    Store store_;
    std::unordered_map<int, Client> clients_;
    static constexpr int MAX_EVENTS{128};

    time_point last_cleanup_;
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{10};

    static constexpr int EPOLL_TIMEOUT_MS = 1000;
};