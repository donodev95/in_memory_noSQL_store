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
