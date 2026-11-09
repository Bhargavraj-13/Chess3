// Placeholder entry point. The real server is issue #9.

#include <iostream>

#include "core/version.hpp"

int main() {
    std::cout << "chess_server placeholder, core version " << chess::core::version() << '\n';
    return 0;
}
