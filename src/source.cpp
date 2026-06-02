#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kPort = 1234;
constexpr int kBacklog = SOMAXCONN;
constexpr std::size_t kBufferSize = 64;

void logMessage(std::string_view message) {
    std::cerr << message << '\n';
}

[[noreturn]] void throwSystemError(std::string_view message) {
    throw std::runtime_error(
        std::string(message) + ": " + std::strerror(errno)
    );
}

class Socket {
public:
    explicit Socket(int fd = -1) noexcept
        : fd_(fd) {}

    ~Socket() {
        close();
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept
        : fd_(other.fd_) {
        other.fd_ = -1;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }

        return *this;
    }

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

    [[nodiscard]] bool valid() const noexcept {
        return fd_ >= 0;
    }

    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_;
};

Socket createServerSocket() {
    Socket serverSocket{::socket(AF_INET, SOCK_STREAM, 0)};

    if (!serverSocket.valid()) {
        throwSystemError("socket()");
    }

    int enabled = 1;

    if (::setsockopt(
            serverSocket.get(),
            SOL_SOCKET,
            SO_REUSEADDR,
            &enabled,
            sizeof(enabled)
        ) < 0) {
        throwSystemError("setsockopt()");
    }
    // Configure SocketAddress for listening server.
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(kPort); // Big Endian
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(
            serverSocket.get(),
            reinterpret_cast<const sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) < 0) {
        throwSystemError("bind()");
    }

    if (::listen(serverSocket.get(), kBacklog) < 0) {
        throwSystemError("listen()");
    }
    
    return serverSocket;
}

Socket acceptClient(int serverFd) {
    sockaddr_in clientAddress{};
    socklen_t clientAddressLength = sizeof(clientAddress);

    const int clientFd = ::accept(
        serverFd,
        reinterpret_cast<sockaddr*>(&clientAddress),
        &clientAddressLength
    );

    if (clientFd < 0) {
        throwSystemError("accept()");
    }

    char ipAddress[INET_ADDRSTRLEN]{};

    if (::inet_ntop(
            AF_INET,
            &clientAddress.sin_addr,
            ipAddress,
            sizeof(ipAddress)
        ) != nullptr) {
        std::cout << "New client from "
                  << ipAddress
                  << ':'
                  << ntohs(clientAddress.sin_port)
                  << '\n';
    }

    return Socket{clientFd};
}

void handleClient(int clientFd) {
    std::array<char, kBufferSize> readBuffer{};

    const ssize_t bytesRead = ::read(
        clientFd,
        readBuffer.data(),
        readBuffer.size() - 1
    );

    if (bytesRead < 0) {
        logMessage("read() error");
        return;
    }

    readBuffer[static_cast<std::size_t>(bytesRead)] = '\0';

    std::cout << "Client says: "
              << readBuffer.data()
              << '\n';

    const std::string_view response = "world";

    const ssize_t bytesWritten = ::write(
        clientFd,
        response.data(),
        response.size()
    );

    if (bytesWritten < 0) {
        logMessage("write() error");
    }
}

} // namespace

int main() {
    try {
        const Socket serverSocket = createServerSocket();

        std::cout << "Server listening on port "
                  << kPort
                  << '\n';

        while (true) {
            Socket clientSocket = acceptClient(serverSocket.get());

            handleClient(clientSocket.get());

            // clientSocket is automatically closed here
            // because of RAII.
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: "
                  << error.what()
                  << '\n';

        return 1;
    }

    return 0;
}