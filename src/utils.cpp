#include <string_view>
#include "utils.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <assert.h>

void logMessage(std::string_view message) {
    std::cerr << message << "\n";
}

void throwSystemError(std::string_view message) {
    throw std::runtime_error(std::string(message) + ": " + std::strerror(errno));
}

int readFull(int fd, uint8_t* buffer, std::size_t bytesToRead) {
    while (bytesToRead > 0) {
        const ssize_t bytesRead = ::read(fd, buffer, bytesToRead);

        if (bytesRead <= 0) {
            return -1; // error or EOF
        }

        assert(static_cast<std::size_t>(bytesRead) <= bytesToRead);

        bytesToRead -= static_cast<std::size_t>(bytesRead);
        buffer += bytesRead;
    }

    return 0;
}

int writeAll(int fd, const uint8_t* buffer, std::size_t bytesToWrite) {
    while (bytesToWrite > 0) {
        const ssize_t bytesWritten = ::write(fd, buffer, bytesToWrite);

        if (bytesWritten <= 0) {
            return -1; // error
        }

        assert(static_cast<std::size_t>(bytesWritten) <= bytesToWrite);

        bytesToWrite -= static_cast<std::size_t>(bytesWritten);
        buffer += bytesWritten;
    }

    return 0;
}