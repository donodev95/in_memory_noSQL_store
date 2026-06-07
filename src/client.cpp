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

    enum class ResponseStatus : uint32_t {
        Ok = 0,
        Error = 1,
        NotFound = 2
    };

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

        const uint32_t responseLength =
            ntohl(encodedResponseLength);

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

        if (response.size() < kHeaderSize) {
            logMessage("bad response");
            return -1;
        }

        uint32_t encodedStatus = 0;

        std::memcpy(
            &encodedStatus,
            response.data(),
            kHeaderSize
        );

        const auto status = static_cast<ResponseStatus>(
            ntohl(encodedStatus)
        );

        const std::string_view data{
            reinterpret_cast<const char*>(response.data() + kHeaderSize),
            response.size() - kHeaderSize
        };

        std::cout << "status=" << static_cast<uint32_t>(status)
                  << ", data=" << data
                  << '\n';

        return 0;
    }
}

int main() {
    Socket clientSocket = createClientSocket();

    connectToServer(clientSocket.get());

    const std::vector<std::vector<std::string>> requests{
        {"set", "name", "dono"},
        {"get", "name"},
        {"del", "name"},
        {"get", "name"}
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