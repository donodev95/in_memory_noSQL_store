# Chapter 3 - Simple TCP Server - Client.
## Server:
- A Simple TCP server-client, which allows 2 Nodes perform socket connections to transfer data.
- Server performs:
    - Allocating Listening Socket (Fd 3).
    - Waiting for connection requests from client and accept it.
    - Reading whatever client sent and generate response.
Code Structure: 
    - Create a socket server - fd 3 (Socket address = IPv4/ IPv6 address + Port)
    - Make the socket listens for connection request.
    - Accept connection requests:
        * Define the client Socket address object which contains client socket detail.
        * Tell the system the size of that socket address to reserve space.
        * Allocate a socket for that connection request.
    - List of Socket on Server Side after accepting one connection request should look like:
    [listening socket - Fd 3][connection socket - Fd 4]...
## Client:
- Client performs:
    - Create Client Socket.
    - Connect to server.
    - Generate and Send message.
## Data Flow:
client application 
-> write() to writeBuffer 
-> TCP converts message (Eg: "hello") to TCP packet (Eg: [68 65 6c 6c 6f]) & sends
-> Network

Server kernel
-> Reives TCP packet.
-> Move TCP packet to receive buffer (which is associated to a client socket - Eg: Fd 4)
-> Kernel copies TCP packet from Receive buffer to ReadBuffer (local buffer in Application layer).
-> Process the business logic
-> generate response.

# Chapter 4: Request-Response Protocol.
In Chapter 3, the server accept 1 connection and perform 1 read with one message only.
Goals:
    - The Server handles 1 connection and perform reading multiple messages from that connection.
Approach:
    - The messages from client will look like this in ByteStream:
        [header1][message1][header2][message2]...
        - Header: 4 bytes in size and contains the size (in bytes) of the message.
        Eg: [00000005]["h" "e" "l" "l" "o"][00000004]["d" "0" "n" "o"][header n][message n] ...
        +----+----+----+----+----+----+----+----+----+
        | 05 | 00 | 00 | 00 | 68 | 65 | 6c | 6c | 6f |
        +----+----+----+----+----+----+----+----+----+
    - This approach allows the system to know the boundaries of each message.
Challenges:
    - The TCP Packets can be splitted. Eg: [00000005]["h" "e"] and ["l" "l" "o"].
    - The TCP only knows the byte stream, so kernel will returns what it has in TCP receive buffer.
    - if the first package arrives ([00000005]["h" "e"]), the kernel will copy [00000005]["h" "e"] into readBuffer. 
    - you are in charge of splitting the stream into header and message body.
    so readfull is required.
    - Writefull is also required. Write buffer is limited -> the caller might need to wait for it to drain. During the wait, the syscall might be interrupted by the signal.

# Chapter 5 + 6: Concurrent IO Models + Event Loops
In the previous approach, one server can accept connections from different clients, each client and holds the connection as long as they want. The connections will be processed one by one, so if connection is accepted, but not sending any data and remains holding the connection -> other connections won't be processed -> it blocks the system.
0. Original Approach:
Blocking model:
    - Eg: Client sends "set name dono"
    - The server does: accept() > read() > parse() > execute() > write() > close()
    - Challenge: If the client sends 100MB -> TCP might split the data into different packets (packet1 - 100Kb, packet2 - 100kb, ...) So, the first execution read() may only be able to copy the packet1 => the pipeline stucks at parse(). 

1. Approach 1:
Thread based IO:
    - one thread handles one client connection (one connection socket)
    => expensive computing, hard to manage resouces (memory) per socket.

2. Approach 2:
Event based IO:
    - TCP stack handles transfering data between nodes and placing data at kernel-receive buffer.
    - read() copies data from kernel-size buffer to local read buffer -> when the kernel-receive buffer is empty -> read() suspends the calling.
    - write() also just put the data into the kernel size buffer for the TCP stack to consume, and when the kernel receive buffer is full, write() suspends the calling threads until there is room.
    - Components:
        * readiness_notification: waits for multiple sockets, return when one or more are "ready". "ready" means either read buffer is not empty or write buffer is not empty.
        * non-blocking read: asuming read buffer is not empty -> consume.
        * non-blocking write: asuming write buffer is not full -> consume.
=> Why this apporach is better? it allows a single client request not having to be completed in a single pass through loop (iteration). Instead, the execution of client request can be spanned between different iterations.

# Operation Diagram:
Eg: Client sends "hello" = [68 65 6c 6c 6f]
* client: 
    Generate Message 
    -> copy message into a local write buffer 
    -> "TCP sending kernel" copies data from local write buffer [00 00 00 05 68 65 6c 6c 6f]
    -> "TCP sending kernel" splits the messages into diffent packets.
    -> networks transfering data.
* server: 
    -> "TCP receive kernel" receives the packets (either a full packet or multiple partial content packets).
    -> "TCP receive kernel" checks the source and destination, maps the source socket address (IPv4 address + port) to the corresponding connected socket. The Data then placed into the receive buffer associated with that connection's socket.
    -> when application calls read(), kernel copies bytes from the socket receive buffer into application's buffer. 

# Chapter 7: Key+Value Server.
Request-Response Protocol:
    server: [Header1 - 4 bytes][MessageBody1][Header2 - 4 bytes][MessageBody2]...
Key:value store:
    server: [4-byte nstr - total number of messages][Header1 - 4 bytes][MessageBody1][Header2 - 4 bytes][MessageBody2]...
    client: [4-byte response length][4-byte status][data]
    
