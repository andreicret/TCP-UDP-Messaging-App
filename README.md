# Client-Server Application for TCP and UDP communication
**Copyright 2024  
Alexandru-Andrei CRET
(alexandru.cret04@gmail.com)**

---

## DESCRIPTION

The purpose of this project is to implement the communication between UDP and TCP clients via a proxy server that manages all connections.

An implementation in C++ language was chosen due to its flexibility and data structures (e.g., unordered_map, strings). The starting point of this project was the seventh lab [1] from PCOM, which was initially done in C.

---

## CONTENTS

- Makefile  
- server.cpp (server functionality)  
- subscriber.cpp (tcp client functionality)  
- common.cpp (additional functions - send/receive properly, wildcard logic)  
- common.h (structures used for data transmission)  
- helpers.h (DIE function used for defensive programming)

---

## SERVER

After textual parsing from stdin, a TCP and UDP socket are requested for receiving TCP and UDP connections. They are reusable thanks to setting the `SO_REUSEADDR` flag with `setsockopt()`. `bind()` links the sockets with the server's IP address.

All connections are stored in a resizable array, `poll_fds*`, facilitating connections with as many clients as possible. The `clients` and `topics` hashmaps are declared globally and used to store the correspondence between socket → client_info and client → topics.

If a new TCP client wants to connect, we check that the current ID has not been used before (it expects an `id_packet` and searches in the `clients` hashmap). If everything is valid, we add the client to the `clients` hashmap and to `poll_fds` (expanding the array size if necessary).

If a UDP client sends a message, the message is forwarded only to the clients subscribed to that topic (pattern matching is used here). The UDP's IP and port are sent beforehand.

When a TCP client sends a command, `tcp_communication()` parses the command using a `subscriber_message`.

The only command received from stdin is `"exit"`, where `disconnect_server()` plays the main role. It sends each TCP client a notification to close their sockets and then closes all remaining connections.

---

## TCP CLIENT

After textual parsing from stdin, a TCP socket is requested to connect to the server. If the connection is active and stable, the client can take commands from stdin or receive messages unless its ID matches another connected client (any message received from the server that starts with IP=0 and PORT=0 signals that something is wrong and the client should disconnect).

Non-blocking functionality is achieved using `poll()` and an array of `struct pollfd`.  
`get_message()` parses input from stdin and decides which command was sent. For all commands (`exit`, `subscribe`, `unsubscribe`), a signal is sent to the server.

When data is received, it can be either an exit signal (invalid sender IP/port) or a message from the UDP client. `parse_message()` facilitates correct parsing and printing.

---

## ADDITIONAL FUNCTIONS AND STRUCTURES

- `recv_all()` and `send_all()` ensure safe data transfer between sockets, preventing data truncation or concatenation that may occur with simple send/recv.
  
- `is_match()` checks if a topic received via UDP matches a wildcard topic the TCP client is subscribed to. The wildcard matching algorithm is based on [2], comparing multiple `std::string`. Because topics are delimited by `/` (not individual characters), `separate()` was implemented to return an array of strings extracted based on `/`, `*`, or `+`.

- `DIE` is a standard macro for defensive programming.

- `struct id_packet` is used to transmit the TCP client's ID to the server.

- `struct client` contains client information and helps the server process clients more easily (used in the `clients` hashmap).

- `struct subscriber_message` is sent by the TCP client to notify the server of its intentions (exit, subscribe, unsubscribe).

- `struct server_message` is used for receiving UDP data and sending it to the TCP client in order to be parsed and printed correctly.


[1]: https://pcom.pages.upb.ro/labs/lab7/lecture.html  
[2]: https://www.geeksforgeeks.org/wildcard-pattern-matching/
