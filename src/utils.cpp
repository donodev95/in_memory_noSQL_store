#include <string_view>
#include "utils.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>

void logMessage(std::string_view message) {
    std::cerr << message << "\n";
}

void throwSystemError(std::string_view message) {
    throw std::runtime_error(std::string(message) + ": " + std::strerror(errno));
}