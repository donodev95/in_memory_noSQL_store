#include <iostream>
#include <cerrno>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#include "utils.hpp"
#include "socket.hpp"

namespace {
    const int kPort = 1234;
    const std::size_t kBufferSize = 64;
    const std::size_t kMaxMessageSize = 4096;
    const std::size_t kHeaderSize = 4;

    Socket createClientSocket () {
        Socket clientSocket{::socket(AF_INET, SOCK_STREAM, 0)};

        if (!clientSocket.valid()) throwSystemError("socket()");
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

    void sendMessage(int ClientFd, std::string_view message) {
        const ssize_t byteWritten = ::write(
            ClientFd,
            message.data(),
            message.size()
        );
        if (byteWritten < 0) throwSystemError("write()");
    }

    std::string readResponse(int clientFd) {
        std::array<char, kBufferSize> readBuffer{};
        const ssize_t byteReads = ::read(
            clientFd,
            readBuffer.data(),
            readBuffer.size() - 1
        );
        if (byteReads < 0) throwSystemError("read()");
        return std::string {
            readBuffer.data(),
            static_cast<std::size_t>(byteReads)
        };
    }

    int query(int fd, std::string_view message) {
        if (message.size() > kMaxMessageSize) {
            logMessage("Message too long");
            return -1;
        }
        const uint32_t messageLength = message.size();

        std::array<char, kHeaderSize + kMaxMessageSize> writeBuffer{};
        std::memcpy(writeBuffer.data(), &messageLength, kHeaderSize);
        std::memcpy(writeBuffer.data() + kHeaderSize, message.data(), messageLength);

        if(writeAll(fd, writeBuffer.data(), kHeaderSize + message.size()) < 0) {
            logMessage("write() error");
            return -1;
        }

        std::array<char, kHeaderSize + kMaxMessageSize> readBuffer{};
        uint32_t responseLength = 0;
        if (readFull(fd, readBuffer.data(), kHeaderSize) < 0) {
            logMessage(errno == 0 ? "EOF" : "read() error");
            return -1;
        }
        // update the responseLength with message length from header.
        std::memcpy(&responseLength, readBuffer.data(), kHeaderSize); 

        if (responseLength > kMaxMessageSize) {
            logMessage("response too long");
            return -1;
        }

        if (readFull(fd, readBuffer.data() + kHeaderSize, responseLength) < 0) {
        logMessage("read() error");
        return -1;
        }

        std::string_view response{
            readBuffer.data() + kHeaderSize,
            responseLength
        };

        std::cout << "Server: " << response << '\n';

        return 0;
    };
}
int main() {
    Socket clientSocket = createClientSocket();
    connectToServer(clientSocket.get());
    if (query(clientSocket.get(), "hello1") < 0) {
        return 1;
    }

    if (query(clientSocket.get(), "hello2") < 0) {
        return 1;
    }

    if (query(clientSocket.get(), "hello3") < 0) {
        return 1;
    }
    if (query(clientSocket.get(), "hello4") < 0) {
        return 1;
    }
    return 0;
}