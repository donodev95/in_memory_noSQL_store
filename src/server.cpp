/* 
Chapter 9: Serialization.
So far, the response is a string, but some redis commands return different type of data, such as integers, list of strings, etc. So it is required to have serialization, which returns the flat, sequential stream of bytes.
    - Simple Data type: string/ integet/ float/ boolean/ null.
    - Complex Data type: array/ map/ struct.

TAG-LENGTH-VALUE (TLV) serialization format:
    - Eg: response = [123, "foo"]
    -> TLV(response) =  [array  2   int     123     str     3       foo]
                        [tag    len tag     value   tag     len   value]
    
    We Implement these data types:
        enum {
            TAG_NIL = 0,    // nil
            TAG_ERR = 1,    // error code + msg
            TAG_STR = 2,    // string
            TAG_INT = 3,    // int64
            TAG_DBL = 4,    // double
            TAG_ARR = 5,    // array
        };
So, the response from server will be changed from:
    [length][status][raw string data]
to: 
    [length][serialized body]

main{
    Create ocketServer.
    main loop {
        add listening socket to pollFds (This poll() will check the connection request queue. if the queue is not empty -> ready -> POLLIN)
        loop through the current active connection sockets.
            append the active connection sockets to pollFds.
        Loop through the waiting to be accepted request queue.
            accept the connection request.
            make the connection socket to non blocking mode.
            create a socket object for the connection socket.
        loop through connection socket in pollFds.
            Extract Ready.
            handleRead()
                create a readBuffer
                Extract byteRead
                handle cases (byteRead < 0 or byteRead == 0)
                append byteRead to connection.incoming.
                handleOneRequest()
                    Extract OuterLength
                    validate amount of data in connection.incoming.
                    parse request to extract command.
                    do business request.
                    generate response.
        handleWrite   
        }
    }

}
*/

#include <iostream>
#include <cerrno>


#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>

#include "utils.hpp"
#include "socket.hpp"
#include <assert.h>
#include "hashtable.hpp"

namespace {
    const int kPort = 1234;
    const int kBacklog = SOMAXCONN;
    const size_t kReadBufferSize = 64*1024;
    const std::size_t kMaxMessageSize = 4096;
    const size_t kHeaderSize = 4;
    const size_t kMaxArguments = 1024;

    #define container_of(ptr, T, member) \
        ((T*)((char*)(ptr) - offsetof(T, member)))

    struct Entry {
        HNode node;
        std::string key;
        std::string value;
    };
    HMap gData; // Global KV data store.
    
    class Connection {
        public:
            explicit Connection(Socket socket) : socket{std::move(socket)} {}
            Socket socket;
            bool wantRead{true};
            bool wantWrite{false};
            bool wantClose{false};
            std::vector<uint8_t> incoming;
            std::vector<uint8_t> outgoing;
        };

    void responseBegin(Buffer& out, std::size_t* headerPosition) {
        *headerPosition = out.size();
        bufAppendU32(out, 0);
    }

    std::size_t responseSize(Buffer& out, std::size_t headerPosition) {
        return out.size() - headerPosition - kHeaderSize;
    }

    void responseEnd(Buffer& out, std::size_t headerPosition) {
        std::size_t size = responseSize(out, headerPosition);

        if (size > kMaxMessageSize) {
            out.resize(headerPosition + kHeaderSize);
            outErr(out, ERR_TOO_BIG, "response is too big");
            size = responseSize(out, headerPosition);
        }

        const uint32_t encodedSize = static_cast<uint32_t>(size);
        std::memcpy(&out[headerPosition], &encodedSize, kHeaderSize);
    }

    bool cbKeys(HNode* node, void* arg) {
        Buffer& out = *static_cast<Buffer*>(arg);
        const Entry* entry = container_of(node, Entry, node);
        outStr(out, entry->key.data(), entry->key.size());
        return true;
    }
    
    
    void setNonBlocking(int fd) {
        /* 
        Check current status of the File Descriptor.
        fcntl = File Control.
        Non-Blocking flag determines the behaviours of the socket.
        - Blocking Socket: if read() -> kernel buffer is empty -> thread sleeps -> wait for network -> data arrives -> read() & return
        - Non-Blocking socket: Read() -> empty kernel buffer -> return immediately -> errno = EAGAIN.
        */
        const int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            throwSystemError("fcntl(F_GETFL)");
        }

        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { // Set flags = current flags + O_NONBLOCK flag.
            throwSystemError("fcntl(F_SETFL)");
        }
    }
    
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
        
        setNonBlocking(serverSocket.get());
        if (::listen(serverSocket.get(), kBacklog) < 0) throwSystemError("listen()");
    
        return serverSocket;
    }

    std::unique_ptr<Connection> handleAccept(int listenFd) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const int connectionFd = accept(
            listenFd,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength
        );

        if (connectionFd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
            logMessage("accept() failed");
            }
            return nullptr;
        }
        setNonBlocking(connectionFd);
        // Convert Binary IP to human readable IP - Eg: C0 A8 01 14 -> 192.168.1.20
        char ipAddress[INET_ADDRSTRLEN]{};
        inet_ntop( 
            AF_INET,
            &clientAddress.sin_addr,
            ipAddress,
            sizeof(ipAddress)
        );
        std::cout << "New client from "
              << ipAddress
              << ':'
              << ntohs(clientAddress.sin_port)
              << '\n';

        return std::make_unique<Connection>(Socket{connectionFd});
    }
    
    void appendBuffer(
        std::vector<uint8_t>& buffer,
        const uint8_t* data,
        std::size_t length
    ) {
        buffer.insert(buffer.end(), data, data + length);
    }

    void consumeBuffer(std::vector<uint8_t>& buffer, std::size_t length) {
        assert(length <= buffer.size());
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(length));
    }
    
    bool readUint32(const uint8_t*& cursor, const uint8_t* end, uint32_t& output) {
        if (cursor + kHeaderSize > end) {
            return false; // Not Enough Data, wait for the next read()
        }

        uint32_t encodedValue = 0;
        std::memcpy(&encodedValue, cursor, kHeaderSize);
        output = ntohl(encodedValue);

        cursor += kHeaderSize;
        return true;
    }
    
    bool readString(
        const uint8_t*& cursor,
        const uint8_t* end,
        uint32_t length,
        std::string& output
    ) {
        if (cursor + length > end) {
            return false;
        }

        output.assign(
            reinterpret_cast<const char*>(cursor),
            length
        );

        cursor += length;
        return true;
    }

    int parseRequest(const uint8_t* data, std::size_t size, std::vector<std::string> &output) {
        // data= [nstr][len][set][len][name][len][dono] = 27 bytes
        const uint8_t* cursor = data;
        const uint8_t* end = data + size;

        uint32_t argumentCount = 0; // read nstr
        if(!readUint32(cursor, end, argumentCount)) {return -1;} 
        
        if(argumentCount > kMaxArguments) {return -1;}  // avoid too much message, which consumes up memory.

        output.clear();
        output.reserve(argumentCount);

        while(output.size() < argumentCount) {
            uint32_t length = 0;
            if (!readUint32(cursor, end, length)) {return -1;}
                
            std::string argument;

            if (!readString(cursor, end, length, argument)) {return -1;}
            output.push_back(std::move(argument));
        }
        if (cursor != end) {
            return -1;
        }
        return 0;
    }
    
    bool entryEqual(HNode* lhs, HNode* rhs) {
    const Entry* left = container_of(lhs, Entry, node);
    const Entry* right = container_of(rhs, Entry, node);
    return left->key == right->key;
    }

    uint64_t stringHash(const uint8_t* data, std::size_t length) {
        uint32_t hash = 0x811C9DC5;

        for (std::size_t i = 0; i < length; ++i) {
            hash = (hash + data[i]) * 0x01000193;
        }

        return hash;
    }

    Entry makeLookupKey(const std::string& keyText) {
        Entry key;
        key.key = keyText;
        key.node.hcode = stringHash(
            reinterpret_cast<const uint8_t*>(key.key.data()),
            key.key.size()
        );
        return key;
    }

    void doRequest(std::vector<std::string>& command, Buffer& out) {
        if (command.size() == 1 && command[0] == "keys") {
            outArr(out, static_cast<uint32_t>(hm_size(&gData)));
            hm_foreach(&gData, cbKeys, &out);
            return;
        }
        if (command.size() == 2 && command[0] == "get") {
            Entry key = makeLookupKey(command[1]);
            HNode* node = hm_lookup(&gData, &key.node, entryEqual);

            if (!node) {
                return outNil(out);
            }

            const Entry* entry = container_of(node, Entry, node);
            return outStr(out, entry->value.data(), entry->value.size());
        }

        if (command.size() == 3 && command[0] == "set") {
            Entry key = makeLookupKey(command[1]);
            HNode* node = hm_lookup(&gData, &key.node, entryEqual);

            if (node) {
                Entry* entry = container_of(node, Entry, node);
                entry->value = std::move(command[2]);
            } else {
                auto* entry = new Entry{};
                entry->key = std::move(command[1]);
                entry->value = std::move(command[2]);
                entry->node.hcode = key.node.hcode;

                hm_insert(&gData, &entry->node);
            }

            return outNil(out);
        }

        if (command.size() == 2 && command[0] == "del") {
            Entry key = makeLookupKey(command[1]);
            HNode* node = hm_delete(&gData, &key.node, entryEqual);

            if (node) {
                delete container_of(node, Entry, node);
            }

            return outInt(out, node ? 1 : 0);
        }
        
        if (command.size() == 1 && command[0] == "dbsize") {
            return outInt(out, static_cast<int64_t>(hm_size(&gData)));
        }

        return outErr(out, ERR_UNKNOWN, "unknown command");
    }
    
    bool handleOneRequest(Connection& connection) {
        // incoming = [outer_length][nstr][header1][body1][header2][body2][header3][body3]
        /* eg:      [outer_length]      [=27(4byte)]
                    [nstr]              [3(4byte)]
                    [header11=]         [3(4byte)]
                    [body1]             [set(3byte)]
                    [header2]           [4(4byte)]
                    [body2]             [name(4byte)]
                    [header3]           [4(4byte)]
                    [body3]             [dono(4byte)] 
        */
        if (connection.incoming.size() < kHeaderSize) {
            return false; // not enough data.
        }

        uint32_t rawMessageLength = 0; // Extract outer-length in byte

        std::memcpy(
            &rawMessageLength,
            connection.incoming.data(),
            kHeaderSize
        );

        const uint32_t messageLength = ntohl(rawMessageLength); // convert Outer Length - Total of bytes for the current message.

        if (messageLength > kMaxMessageSize) {
            logMessage("Message too long");
            connection.wantClose = true;
            return false;
        }

        const std::size_t fullMessageSize = kHeaderSize + messageLength; // is the total of bytes of the message.
        // Eg: [outerLength = 27][nstr = 3 (3 items)][3][set][4][name][4][dono] -> the message body + nstr = 27 bytes
        // fullMessageSize = outerLength (4-byte) + 27 byte = 31

        if (connection.incoming.size() < fullMessageSize) {
            return false; // Not Enough data.
        }

        const uint8_t* requestBodyBegin = connection.incoming.data() + kHeaderSize; // move the pointer to the begin of body (pass nstr + header)

        std::vector<std::string> command; // Extract tge command

        if (parseRequest(requestBodyBegin, messageLength, command) < 0) { // parse the request.
            logMessage("Bad request");
            connection.wantClose = true;
            return false;
        }

        std::size_t headerPosition = 0;

        responseBegin(connection.outgoing, &headerPosition);

        doRequest(command, connection.outgoing);

        responseEnd(connection.outgoing, headerPosition);

        consumeBuffer(connection.incoming, fullMessageSize); // remove the processed message from the buffer.

        return true;
    }

    void handleWrite(Connection& connection) {
        if (connection.outgoing.empty()) {
            connection.wantWrite = false;
            connection.wantRead = true;
            return;
        }

        const ssize_t bytesWritten = ::write(
            connection.socket.get(),
            connection.outgoing.data(),
            connection.outgoing.size()
        );

        if (bytesWritten < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            logMessage("write() failed");
            connection.wantClose = true;
            return;
        }

        consumeBuffer(
            connection.outgoing,
            static_cast<std::size_t>(bytesWritten)
        );

        if (connection.outgoing.empty()) {
            connection.wantRead = true;
            connection.wantWrite = false;
        }
    }

    void handleRead(Connection& connection) {
        std::array<uint8_t, kReadBufferSize> buffer{};
        const ssize_t bytesRead = read(
            connection.socket.get(),
            buffer.data(),
            buffer.size()
        );
        
        if(bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
            }

            logMessage("read() failed");
            connection.wantClose = true;
            return;
        }

        if (bytesRead == 0) {
            if (connection.incoming.empty()) {
                std::cerr << "Client closed connection\n";
            } else {
                std::cerr << "Unexpected EOF\n";
            }

            connection.wantClose = true;
            return;
        }

        appendBuffer(
            connection.incoming,
            buffer.data(),
            bytesRead
        ); // append all the client's messages sent to the current connection socket to the connection.incoming - byte stream of different messages now.

        while(handleOneRequest(connection)) {} // continuously process the messages until can't process anymore.
        
        if (!connection.outgoing.empty()) {
        connection.wantRead = false;
        connection.wantWrite = true;
        // After done reading, assuming the write kernel is not full -> write the response immediately without waiting for the next iteration.
        handleWrite(connection);
        }
    }
}

int main() {
    Socket listenSocket = createSocketServer();
    std::cout << "Server Listening on port: " << kPort << std::endl;
    /* 
    a vector (use heap memory -> dynamically allocation and is scalable as runtime) of socket. using type smart pointer which owns a connection object for automatically executing "delete" later.
    */
    std::vector<std::unique_ptr<Connection>> fd2conn; // key: index, value: connection.
    std::vector<pollfd> pollFds{}; // a list of polls. each poll watches one socket.

    while(true) {
        pollFds.clear();
        // add the listening (server) socket to the poll. set events for watching to POLLIN (read).
        pollFds.push_back(pollfd{ // This Poll checks the listening socket if there is any connection request in the queue. if yes -> revents = POLLIN
            .fd = listenSocket.get(),
            .events = POLLIN,
            .revents = 0    
        });
        
        for (const auto& connection : fd2conn) { // need to use actual value of connection, not a copy version -> type + & is required. auto type let compiler automatically setting the type of object (Eg: when extracts connection from the list or when connection is terminated -> type = )
            if (!connection) continue;

            short events = 0; // socket error
            if (connection->wantRead) {
                events |= POLLIN; // there are data waiting to read for the current socket in kernel receive buffer.
            }
            if (connection->wantWrite) {
                events |= POLLOUT; // There is more space to write in kernel send buffer.
            }
            // append the connection socket to poll vector.
            pollFds.push_back(pollfd{
                .fd = connection -> socket.get(),
                .events = events,
                .revents = 0
            });
        }

        const int readyCount = ::poll(
            pollFds.data(),
            pollFds.size(),
            -1
        );

        if (readyCount < 0) {
            if (errno == EINTR) {
                continue;
            }

            throwSystemError("poll()");
        }

        /* Working on the Listening Socket */
        if (pollFds[0].revents & POLLIN) { // bit manipulation.
            // There might be more than 1 client requests sent to server at a time, Everytime handleAccept() is executed, one request will be pulled out from the stack by accept() to be processed. Loop until no more request -> return null.
            while(auto connection = handleAccept(listenSocket.get())) { // a temporary connection variable.
                const int connectionFd = connection->socket.get();

                if(fd2conn.size() <= static_cast<std::size_t>(connectionFd)) {
                    fd2conn.resize(static_cast<std::size_t>(connectionFd) + 1);
                }

                assert(!fd2conn[static_cast<std::size_t>(connectionFd)]);
                fd2conn[static_cast<std::size_t>(connectionFd)] = std::move(connection); // Move the ownership of connection to Fd2conn connection socket index.
            }
        }
        for (std::size_t i = 1; i < pollFds.size(); ++i) // Process connection sockets
        {
            const int fd = pollFds[i].fd;
            const short ready = pollFds[i].revents;

            if (ready == 0) continue;

            assert(ready > 0);
            assert(static_cast<std::size_t>(fd) < fd2conn.size());

            Connection* connection = fd2conn[fd].get();
            if(!connection) continue;

            // handle read
            if (ready & POLLIN) {
                assert(connection->wantRead);
                handleRead(*connection);
            }
            // Handle wrire
            if (ready & POLLOUT) {
                assert(connection->wantWrite);
                handleWrite(*connection);
            }
            // Close the socket from socket error or application logic
            if ((ready & POLLERR) || connection->wantClose) {
                std::cout << "Client disconnected\n";
                fd2conn[fd].reset();
            }
        }
        
    }
    
    return 0;
}
