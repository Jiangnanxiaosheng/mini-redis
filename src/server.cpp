#include "server.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

Server::Server(int port) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to set socket options");
    }

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
}

Server::~Server() { close(server_fd_); }

void Server::run() {
    int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
        std::cerr << "Accept failed\n";
        return;
    }

    // Command processing loop
    char buffer[1024];
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0)
            break;  // Client disconnected or error
        buffer[bytes_read] = '\0';

        std::string command(buffer);
        std::string response = Command::process(command, store_);
        if (send(client_fd, response.c_str(), response.size(), 0) < 0) {
            std::cerr << "Send failed\n";
            break;
        }
    }

    close(client_fd);
}