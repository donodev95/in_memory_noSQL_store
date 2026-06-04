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