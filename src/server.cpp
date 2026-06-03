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
    const int kBacklog = SOMAXCONN;
    const size_t kBufferSize = 64;
    const std::size_t kMaxMessageSize = 4096;

    Socket createSocketServer() {
        Socket serverSocket{socket(AF_INET, SOCK_STREAM, 0)}; // Instantiate serverSocket with IPv4 protocol, and TCP type.
        if (!serverSocket.valid()) throwSystemError("Socket()");

        int enabled = 1;
        if  (setsockopt(serverSocket.get(),
                SOL_SOCKET,
                SO_REUSEADDR,
                &enabled,
                sizeof(enabled)) < 0
            ) {
                throwSystemError("setsockopt");
            }
        // Configure Socket address
        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(kPort);
        serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        
        if (bind(
            serverSocket.get(),
            reinterpret_cast<const sockaddr*>(&serverAddress),
            sizeof(serverAddress)
            ) < 0) 
            {
                throwSystemError("bind()");
            }

        if (::listen(serverSocket.get(), kBacklog) < 0) throwSystemError("listen()");
    
        return serverSocket;
    }
    
    Socket acceptClient(int serverFd) {
        sockaddr_in clientAddress = {};
        socklen_t clientAddressLength = sizeof(clientAddress); 
        // Tell accept how big of the buffer for client address is. Accept the client request and create a new socket for that client.
        const int clientFd = accept( 
            serverFd,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength
        );
        if(clientFd < 0) throwSystemError("accept()");
    
        char ipAddress[INET_ADDRSTRLEN]{}; // Create a string buffer for IP address. 
        if (::inet_ntop( // Convert from binary to human readable string.
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
    
    int handleOneRequest(int clientFd) {
        // Reserve [4 bytes header][4096 bytes message body]
        std::array<char, kMaxMessageSize + 4> readBuffer{}; 
        // Read the header
        if (readFull(clientFd, readBuffer.data(), 4) < 0) { 
            logMessage(errno == 0 ? "EOF" : "read() error");
            return -1;
        }

        uint32_t messageLength = 0;
        // Copy msg size in header from readBuffer to messageLength
        std::memcpy(&messageLength, readBuffer.data(), 4); 

        if (messageLength > kMaxMessageSize) {
            logMessage("Message too long");
            return -1;
        }
        // Read the message body.
        if (readFull(clientFd, readBuffer.data(), messageLength) < 0) { // 
            logMessage("read() error");
            return -1;
        }
        /* 
        Print the client message. readBuffer.data() returns the pointer to the first byte in the message.
        +4 to skip the first 4 header bytes.
        */
        std::string_view message{readBuffer.data() + 4, messageLength};
        std::cerr << "Client: " <<message << "\n";
        // Generate Response
        std::string response{"received - "};
        response += message;
        // Generate Write Buffer.
        std::array<char, 4 + 64> writeBuffer{};
        uint32_t responseLength = response.size();
        
        std::memcpy(writeBuffer.data(), &responseLength, 4);
        std::memcpy(writeBuffer.data() + 4, response.data(), response.size());
        return writeAll(
            clientFd,
            writeBuffer.data(),
            4 + response.size()
        );
    }
}


int main() {
    Socket serverSocket = createSocketServer();
    std::cout << "Server Listening on port: " << kPort << std::endl;
    while (true)
    {
        Socket clientSocket = acceptClient(serverSocket.get());
         while(true) {
            const int result = handleOneRequest(clientSocket.get());
            if (result < 0) break;
        }
    }
    
    return 0;
}