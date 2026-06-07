#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "socket.hpp"
#include "utils.hpp"

namespace {
    constexpr int kPort = 1234;
    constexpr std::size_t kMaxMessageSize = 32 << 20;
    constexpr std::size_t kHeaderSize = 4;
    enum {
        TAG_NIL = 0,
        TAG_ERR = 1,
        TAG_STR = 2,
        TAG_INT = 3,
        TAG_DBL = 4,
        TAG_ARR = 5,
    };

    enum class ResponseStatus : uint32_t {
        Ok = 0,
        Error = 1,
        NotFound = 2
    };

    bool readU32(const uint8_t*& cursor, const uint8_t* end, uint32_t& value) {
        if (cursor + 4 > end) {
            return false;
        }

        std::memcpy(&value, cursor, 4);
        cursor += 4;
        return true;
    }

    bool readI64(const uint8_t*& cursor, const uint8_t* end, int64_t& value) {
        if (cursor + 8 > end) {
            return false;
        }

        std::memcpy(&value, cursor, 8);
        cursor += 8;
        return true;
    }

    bool printSerializedValue(const uint8_t*& cursor, const uint8_t* end) {
        if (cursor >= end) {
            return false;
        }

        const uint8_t tag = *cursor++;

        switch (tag) {
            case TAG_NIL:
                std::cout << "(nil)";
                return true;

            case TAG_STR: {
                uint32_t length = 0;
                if (!readU32(cursor, end, length)) {
                    return false;
                }

                if (cursor + length > end) {
                    return false;
                }

                std::cout << '"'
                        << std::string_view{
                                reinterpret_cast<const char*>(cursor),
                                length
                            }
                        << '"';

                cursor += length;
                return true;
            }

            case TAG_INT: {
                int64_t value = 0;
                if (!readI64(cursor, end, value)) {
                    return false;
                }

                std::cout << value;
                return true;
            }

            case TAG_ERR: {
                uint32_t code = 0;
                uint32_t length = 0;

                if (!readU32(cursor, end, code)) {
                    return false;
                }

                if (!readU32(cursor, end, length)) {
                    return false;
                }

                if (cursor + length > end) {
                    return false;
                }

                std::cout << "(err " << code << ") "
                        << std::string_view{
                                reinterpret_cast<const char*>(cursor),
                                length
                            };

                cursor += length;
                return true;
            }

            case TAG_ARR: {
                uint32_t count = 0;
                if (!readU32(cursor, end, count)) {
                    return false;
                }

                std::cout << "[";
                for (uint32_t i = 0; i < count; ++i) {
                    if (i > 0) {
                        std::cout << ", ";
                    }

                    if (!printSerializedValue(cursor, end)) {
                        return false;
                    }
                }
                std::cout << "]";
                return true;
            }

            default:
                return false;
        }
    }

    Socket createClientSocket() {
        Socket clientSocket{::socket(AF_INET, SOCK_STREAM, 0)};

        if (!clientSocket.valid()) {
            throwSystemError("socket()");
        }

        return clientSocket;
    }

    void connectToServer(int clientFd) {
        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(kPort);
        serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::connect(
                clientFd,
                reinterpret_cast<const sockaddr*>(&serverAddress),
                sizeof(serverAddress)
            ) < 0) {
            throwSystemError("connect()");
        }
    }

    void appendBuffer(
        std::vector<uint8_t>& buffer,
        const uint8_t* data,
        std::size_t length
    ) {
        buffer.insert(buffer.end(), data, data + length);
    }

    void appendUint32(std::vector<uint8_t>& buffer, uint32_t value) {
        const uint32_t encodedValue = htonl(value);

        appendBuffer(
            buffer,
            reinterpret_cast<const uint8_t*>(&encodedValue),
            kHeaderSize
        );
    }

    int32_t sendRequest(
        int fd,
        const std::vector<std::string>& command
    ) {
        std::vector<uint8_t> body;

        appendUint32(
            body,
            static_cast<uint32_t>(command.size())
        );

        for (const std::string& argument : command) {
            appendUint32(
                body,
                static_cast<uint32_t>(argument.size())
            );

            appendBuffer(
                body,
                reinterpret_cast<const uint8_t*>(argument.data()),
                argument.size()
            );
        }

        if (body.size() > kMaxMessageSize) {
            logMessage("message too long");
            return -1;
        }

        std::vector<uint8_t> request;

        appendUint32(
            request,
            static_cast<uint32_t>(body.size())
        );

        appendBuffer(
            request,
            body.data(),
            body.size()
        );

        return writeAll(
            fd,
            request.data(),
            request.size()
        );
    }

    int32_t readResponse(int fd) {
        std::array<uint8_t, kHeaderSize> header{};

        errno = 0;

        int32_t error = readFull(
            fd,
            header.data(),
            header.size()
        );

        if (error != 0) {
            logMessage(errno == 0 ? "EOF" : "read() error");
            return error;
        }

        uint32_t encodedResponseLength = 0;

        std::memcpy(
            &encodedResponseLength,
            header.data(),
            kHeaderSize
        );

        const uint32_t responseLength = encodedResponseLength;

        if (responseLength > kMaxMessageSize) {
            logMessage("response too long");
            return -1;
        }

        std::vector<uint8_t> response(responseLength);

        error = readFull(
            fd,
            response.data(),
            response.size()
        );

        if (error != 0) {
            logMessage("read() error");
            return error;
        }

        const uint8_t* cursor = response.data();
        const uint8_t* end = response.data() + response.size();

        if (!printSerializedValue(cursor, end) ||
            cursor != end)
        {
            logMessage("bad serialized response");
            return -1;
        }

        std::cout << '\n';

        return 0;
    }
}

int main() {
    Socket clientSocket = createClientSocket();

    connectToServer(clientSocket.get());

    const std::vector<std::vector<std::string>> requests{
        {"set", "name", "dono"},
        {"get", "name"},
        {"set", "age", "28"},
        {"dbsize"},
        {"keys"},
        {"del", "name"},
        {"get", "name"},
        {"dbsize"},
        {"keys"}
    };

    for (const auto& request : requests) {
        if (sendRequest(clientSocket.get(), request) != 0) {
            return 1;
        }

        if (readResponse(clientSocket.get()) != 0) {
            return 1;
        }
    }

    return 0;
}