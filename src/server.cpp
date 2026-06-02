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

void handleClient(int clientFd) {
    std::array<char, kBufferSize> readBuffer{};

    const ssize_t byteRead = read(
        clientFd,
        readBuffer.data(),
        readBuffer.size() - 1
    );
    if(byteRead < 0) {
        logMessage("read() Error");
        return;
    }
    readBuffer[static_cast<std::size_t>(byteRead)] = '\0';
    std::cerr << "Client: " << readBuffer.data() << "\n";
    const std::string_view response = "world";
    const size_t bytesWritten = write(
        clientFd,
        response.data(),
        response.size()
    );
    if (bytesWritten < 0) logMessage("write() error");
}

int main() {
    Socket serverSocket = createSocketServer();
    std::cout << "Server Listening on port: " << kPort << std::endl;
    while (true)
    {
        Socket clientSocket = acceptClient(serverSocket.get());
        handleClient(clientSocket.get());
    }
    
    return 0;
}