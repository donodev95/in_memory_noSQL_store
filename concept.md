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

