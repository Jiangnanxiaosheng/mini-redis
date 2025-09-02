#include "server.hpp"

int main() {
    Server server(6379, "aof.log");
    server.run();

    return 0;
}
