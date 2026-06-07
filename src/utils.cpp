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

void bufAppend(Buffer& buffer, const uint8_t* data, std::size_t length) {
        buffer.insert(buffer.end(), data, data + length);
    }

void bufAppendU8(Buffer& buffer, uint8_t value) {
    buffer.push_back(value);
}

void bufAppendU32(Buffer& buffer, uint32_t value) {
    bufAppend(buffer, reinterpret_cast<const uint8_t*>(&value), 4);
}

void bufAppendI64(Buffer& buffer, int64_t value) {
    bufAppend(buffer, reinterpret_cast<const uint8_t*>(&value), 8);
}

void outNil(Buffer& out) {
        bufAppendU8(out, TAG_NIL);
    }

void outStr(Buffer& out, const char* data, std::size_t size) {
    bufAppendU8(out, TAG_STR);
    bufAppendU32(out, static_cast<uint32_t>(size));
    bufAppend(out, reinterpret_cast<const uint8_t*>(data), size);
}

void outInt(Buffer& out, int64_t value) {
    bufAppendU8(out, TAG_INT);
    bufAppendI64(out, value);
}

void outArr(Buffer& out, uint32_t count) {
    bufAppendU8(out, TAG_ARR);
    bufAppendU32(out, count);
}

void outErr(Buffer& out, uint32_t code, const std::string& message) {
    bufAppendU8(out, TAG_ERR);
    bufAppendU32(out, code);
    bufAppendU32(out, static_cast<uint32_t>(message.size()));
    bufAppend(out, reinterpret_cast<const uint8_t*>(message.data()), message.size());
}

