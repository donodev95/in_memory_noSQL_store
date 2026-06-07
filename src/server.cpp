/* 
Chapter 8: Hashtables
1. Why Hash Table:
    Eg: Look up "apple." There are 2 popular data structure for a KV store.
        - Sorting data structure like Trie, Treap, Tree, etc. (each node contains a value and left and right node.)
            This type of data structure remains order and uses comparisions to narrow the search. O(log n) => 1.000.000 keys require around 20 comparisions.
        - Hashtable: open addressing and chaining. => O(1)
            - A Hash function primary job is to distribute indexes across buckets so collision stays low and look up remains close to O(1).
            Eg: key = "name" with table size = 16 -> hash(name) = 123456. use 123456 % table size = 0 => index 0.
            Eg: the indexes of apple, banana, orange = 1
                - Open addresing:
                    insert("apple") -> index 1.
                    insert("banana") -> index 1 is occupied -> index 2
                    insert("orange") -> index 1 is occupied, index 2 is occupied -> index 3.
                    => open addresing handles collision by moving to the next available nodes, no extra nodes needed.
                    => disadvantage: when delete, Eg: remove banana -> index 2 is empty.
                        1: apple
                        2:
                        3: orange
                        When look up for orange, traverse from index 1 - aplle -> move to index == "empty" -> return no "orange." => failed.
                        So you will need to add a special Marker "DELETED" => more complex algorithms.
                - Chaining: Linked List.
                    0:
                    1: apple (node) -> banana (node) -> orange (node)
                    2:
                    => Eg: remove "banana" => remove banana node, points apple.next to orange.
2. About Hash Table:
    - The Hash Table only contains the list of head nodes of linked lists, not their indexes.
    - Each Hash tabl has a fixed size number of buckets (capacity) - which is normally power of 2.
    - There is a need to rehash the hash table when the amount of key exceeds a certain threshold.
        Eg: load_factor = keys/ capacity. 
            1.000.000 keys / 1000 buckets -> load_factor = 1000 -> requires about 1000 nodes per look up -> slow.
    - Data structure for keys in a hash table:
        Entry {
            HNode node;
            std::string key;
            std::string value;
        }
        HNode{
            HNode* next; -> Pointer to next node, no data being stored here.
            uint_64 hcode; -> the result of hash(key). eg: hash("name") = 892374982374 -> hcode = 892374982374. 
        }
        
3. Reshash table challenges:
    - initiate a table (with null pointers - as requirements of linked list) takes times with a big capacity.
        Eg: 1.000.000 buckets, each bucket is a null pointer (8 bytes) -> 8.000.000 bytes = 8MB to write.
    - moving all buckets from old table to a new table all at once is expensive and takes time.
4. Solution:
    - Manages 2 different tables (old and new)
    - The rehash is triggered when inserting new node. if the size is greater or equal to a certain threshold -> rehash.

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

    HMap gData; // Global KV data store.

    struct Entry {
        HNode node;
        std::string key;
        std::string value;
    };
    
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
    
        enum class ResponseStatus : uint32_t {
        Ok = 0,
        Error = 1,
        NotFound = 2
    };

    struct Response {
        ResponseStatus status{ResponseStatus::Ok};
        std::vector<uint8_t> data;
    };
    
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
    
    void makeResponse(const Response& response, std::vector<uint8_t>& output) {
        const auto dataSize = static_cast<uint32_t>(response.data.size());
        const uint32_t responseLength = kHeaderSize + dataSize;

        const uint32_t encodedResponseLength = htonl(responseLength);
        const uint32_t encodedStatus =
            htonl(static_cast<uint32_t>(response.status));

        appendBuffer(
            output,
            reinterpret_cast<const uint8_t*>(&encodedResponseLength),
            kHeaderSize
        );

        appendBuffer(
            output,
            reinterpret_cast<const uint8_t*>(&encodedStatus),
            kHeaderSize
        );

        if (!response.data.empty()) {
            appendBuffer(
                output,
                response.data.data(),
                response.data.size()
            );
        }
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
    
    void doRequest(std::vector<std::string>& command, Response& response) {
        if (command.size() == 2 && command[0] == "get") {
            Entry key = makeLookupKey(command[1]);
            HNode* node = hm_lookup(&gData, &key.node, entryEqual);

            if (!node) {
                response.status = ResponseStatus::NotFound;
                return;
            }

            const Entry* entry = container_of(node, Entry, node);
            response.data.assign(entry->value.begin(), entry->value.end());
            response.status = ResponseStatus::Ok;
            return;
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

            response.status = ResponseStatus::Ok;
            return;
        }

        if (command.size() == 2 && command[0] == "del") {
            Entry key = makeLookupKey(command[1]);
            HNode* node = hm_delete(&gData, &key.node, entryEqual);

            if (node) {
                delete container_of(node, Entry, node);
            }

            response.status = ResponseStatus::Ok;
            return;
        }

        response.status = ResponseStatus::Error;
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

        Response response;

        doRequest(command, response); // Execute business logic

        makeResponse(response, connection.outgoing); // Generate response

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
