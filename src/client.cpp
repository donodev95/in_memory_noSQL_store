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
}
int main() {
    Socket clientSocket = createClientSocket();
    connectToServer(clientSocket.get());
    const std::string_view message = "hello";
    sendMessage(clientSocket.get(), message);
    const std::string response = readResponse(clientSocket.get());
    std::cout << "Server:" << response << "\n";
    return 0;
}