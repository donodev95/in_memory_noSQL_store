# redis_dono

`redis_dono` is a small Redis-inspired TCP key-value store written in C++.
It implements a non-blocking server, a custom request/response protocol, an
in-memory hash map, and a simple client that sends example commands to the
server.

The project is useful for understanding how Redis-like systems work at a lower
level: sockets, polling, framing messages, serialization, request parsing, and
hash table storage.

## Features

- TCP server listening on port `1234`
- Non-blocking sockets using `fcntl(..., O_NONBLOCK)`
- Event loop based on `poll()`
- Multiple client connection handling
- Length-prefixed request and response messages
- Custom TLV-style response serialization
- In-memory key-value storage
- Progressive hash table rehashing
- RAII socket wrapper for safer file descriptor management
- Example client that sends commands and prints decoded responses

## Supported Commands

The server currently supports these Redis-style commands:

| Command | Description | Example |
| --- | --- | --- |
| `set <key> <value>` | Store or update a string value | `set name dono` |
| `get <key>` | Read a value by key | `get name` |
| `del <key>` | Delete a key. Returns `1` if deleted, `0` if missing | `del name` |
| `dbsize` | Return the number of keys in the database | `dbsize` |
| `keys` | Return all keys | `keys` |

Unknown commands return an error response.

## Architecture

The project is split into a few small modules:

```text
.
├── include/
│   ├── socket.hpp      # RAII wrapper around a socket file descriptor
│   ├── utils.hpp       # I/O helpers, buffer helpers, serialization API
│   └── hashtable.hpp   # Hash table and hash map data structures
├── src/
│   ├── server.cpp      # TCP server, event loop, protocol parsing, commands
│   ├── client.cpp      # Example client and response decoder
│   ├── utils.cpp       # read/write helpers and TLV serialization
│   └── hashtable.cpp   # Hash map implementation with progressive rehashing
├── concept.md          # Learning notes and design concepts
└── main.cpp            # Scratch/demo file, not required for server/client
```

### Server Flow

1. Create a TCP listening socket on port `1234`.
2. Enable `SO_REUSEADDR`.
3. Set the listening socket to non-blocking mode.
4. Enter a `poll()` event loop.
5. Accept new clients and store each connection by file descriptor.
6. Read incoming bytes into each connection's input buffer.
7. Parse complete length-prefixed requests.
8. Execute the command against the global in-memory key-value store.
9. Serialize the response into the connection's output buffer.
10. Write pending response bytes back to the client.

### Storage Layer

The key-value database is stored in a global `HMap`.

Each stored entry contains:

- a hash table node
- a string key
- a string value

The hash map uses separate chaining and grows through progressive rehashing.
Instead of moving every entry at once during resize, each lookup, insert,
delete, or iteration migrates a small amount of work from the old table to the
new table.

### Protocol

Requests use a length-prefixed binary format:

```text
[message_length][argument_count][arg1_length][arg1][arg2_length][arg2]...
```

For example:

```text
set name dono
```

is encoded as:

```text
[body length][3][3]["set"][4]["name"][4]["dono"]
```

Responses are also length-prefixed:

```text
[response_length][serialized_body]
```

The response body uses a TLV-style format:

```text
[tag][length/value...]
```

Supported response tags:

| Tag | Type |
| --- | --- |
| `0` | nil |
| `1` | error |
| `2` | string |
| `3` | int64 |
| `4` | double |
| `5` | array |

## Requirements

- C++17-compatible compiler
- POSIX-like environment with sockets and `poll()`

This project should build on macOS and Linux.

## Build

From the project root, build the server:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude \
  src/server.cpp src/utils.cpp src/hashtable.cpp \
  -o server
```

Build the example client:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude \
  src/client.cpp src/utils.cpp \
  -o client
```

## Run

Start the server in one terminal:

```sh
./server
```

Expected output:

```text
Server Listening on port: 1234
```

Then run the client in another terminal:

```sh
./client
```

The client sends this sequence of example requests:

```text
set name dono
get name
set age 28
dbsize
keys
del name
get name
dbsize
keys
```

Example output:

```text
(nil)
"dono"
(nil)
2
["age", "name"]
1
(nil)
1
["age"]
```

The order returned by `keys` may vary because keys are read from the hash
table.

## Notes

- Data is stored only in memory. Restarting the server clears the database.
- The server currently uses a single process and one `poll()` event loop.
- The example client has a fixed list of requests in `src/client.cpp`.
- Command names are currently matched in lowercase.

