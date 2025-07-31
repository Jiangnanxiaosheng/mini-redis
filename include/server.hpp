#pragma once
#include <string>

#include "command.hpp"
#include "store.hpp"

class Server {
public:
    Server(int port);
    ~Server();
    void run();

private:
    int server_fd_;
    Store store_;
};