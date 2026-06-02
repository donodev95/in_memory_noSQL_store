#pragma once
#include <sys/socket.h>
#include <unistd.h>

class Socket {
public:
    // Constructor
    explicit Socket(int fd = -1) noexcept : fd_{fd} {}
    // destructor
    ~Socket() {
        close();
    }

    Socket(const Socket&) = delete; // Copy constructor - Instantiate Socket by an exisiting Socket.
    Socket& operator=(const Socket&) = delete; // Copy Assignment - Ban Assigning an existing socket to another existing socket.

    Socket(Socket&& other) noexcept :  fd_{other.fd_} { // move constructor - Socket socket2{std::move(socket1)} - Instantiate a socket by an existing socket
        other.fd_ = -1;
    }

    Socket& operator=(Socket&& other) { // Move Assignment Operator: socket2 = std::move(socket1) - assign one existing socket to another exisiting socket
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = -1;
        } 
        return *this;
    }
    
    int get() noexcept {
        return fd_;
    }

    bool valid() noexcept {
        return fd_ >= 0;
    }

    void close() noexcept {
        if (fd_ >= 0) {
            ::close(fd_); // call close() from unistd
            fd_ = -1;
        }
    }

private:
    int fd_{-1};
};
