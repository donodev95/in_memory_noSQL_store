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
    const std::size_t kMaxMessageSize = 32 << 20; // left bitwise shift operator: - 32 MB ~ 33,554,432 z characters
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

    static void appendBuffer(
        std::vector<uint8_t> &buffer,
        const uint8_t *data,
        size_t length
    ) {
        buffer.insert(buffer.end(), data, data + length);
    }

    static int32_t sendRequest(int fd, const std::string &text) {
        if (text.size() > kMaxMessageSize) {
            logMessage("Message too long");
            return -1;
        }

        uint32_t length = static_cast<uint32_t>(text.size());
        uint32_t rawHeaderLength = htonl(length);

        /* Generate message */
        std::vector<uint8_t> writeBuffer;
        // Write Message Header
        appendBuffer(
            writeBuffer,
            reinterpret_cast<const uint8_t *>(&rawHeaderLength),
            4
        );
        // Write Message Body
        appendBuffer(
            writeBuffer,
            reinterpret_cast<const uint8_t *>(text.data()),
            text.size()
        );

        return writeAll(fd, writeBuffer.data(), writeBuffer.size());
    }
    static int32_t readResponse(int fd) {
        std::vector<uint8_t> readBuffer(kHeaderSize);

        errno = 0;

        int32_t error = readFull(
            fd,
            readBuffer.data(),
            kHeaderSize
        );

        if (error != 0) {
            logMessage(errno == 0 ? "EOF" : "read() error");
            return error;
        }

        uint32_t encodedLength = 0;

        std::memcpy(
            &encodedLength,
            readBuffer.data(),
            kHeaderSize
        );

        const uint32_t responseLength = ntohl(encodedLength);

        if (responseLength > kMaxMessageSize) {
            logMessage("response too long");
            return -1;
        }

        readBuffer.resize(kHeaderSize + responseLength);

        error = readFull(
            fd,
            readBuffer.data() + kHeaderSize,
            responseLength
        );

        if (error != 0) {
            logMessage("read() error");
            return error;
        }

        const std::string_view responseBody{
            reinterpret_cast<const char*>(readBuffer.data() + kHeaderSize),
            responseLength
        };

        std::cout << "Server response length: "
                << responseLength
                << ", data: "
                << responseBody.substr(0, 100)
                << '\n';

        return 0;
    }
}
int main() {
    Socket clientSocket = createClientSocket();
    connectToServer(clientSocket.get());
    std::vector<std::string> requests = {
        "hello1",
        "hello2",
        "hello3",
        // std::string(kMaxMsg, 'z'), // Create a kMaxMesg string of character z
        "hello5"
    };
    for (const std::string &request: requests) {
        if (sendRequest(clientSocket.get(), request) != 0) {
            return 1;
        }
    }
    for (size_t i = 0; i < requests.size(); ++i) {
        if(readResponse(clientSocket.get()) != 0) {
            return 1;
        }
    }
    return 0;
}