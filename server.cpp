#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"
#include "helpers.h"
#include <netinet/tcp.h>

using namespace std;

unordered_map <string, vector<string>> topics;
unordered_map <uint32_t, struct client> clients;

/**
 * @brief Close the sockets, free the memory, exit the program smoothly
 * 
 * @param poll_fds the opened sockets
 * @param num_sockets how many sockets are active
 */
void disconnect_server(struct pollfd* poll_fds, size_t num_sockets) {

	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof(cli_addr);

	// Notify the clients to close thir sockets
	for (auto it : clients) {
		cli_addr.sin_addr.s_addr = 0;
		cli_addr.sin_port = 0;

		int rc = send_all(it.first, &cli_addr, sizeof(cli_addr));
		DIE(rc < 0, "Send");
	}

	for (size_t i = 0; i < num_sockets; i++)
		close(poll_fds[i].fd);

	delete(poll_fds);
}

/**
 * @brief Resize the sockets array
 * 
 * @param poll_fds the socket array 
 * @param poll_size current array's size
 * @return struct pollfd* pointer to the new reallocated array
 */
struct pollfd* resize(struct pollfd* poll_fds, size_t* poll_size) {
	*poll_size *= 2;
	struct pollfd* new_poll = (struct pollfd*)realloc(poll_fds, *poll_size);
	DIE(!new_poll, "realloc");

	free(poll_fds);
	poll_fds = new_poll;

	return new_poll;	
}

/**
 * @brief Find the file descriptor corresponding to a given username
 * 
 * @param id the client's id
 */
int find_value(char* id) {
	for (auto it: clients)
		if (!strcmp(it.second.id, id))
			return it.first;

	return -1;
}

/**
 * @brief A tcp client sent a command and it needs to be parsed
 * 
 * @param i the socket index
 * @param poll_fds socket array
 * @param num_sockets the number of sockets
 */
void tcp_communication(size_t i, struct pollfd* poll_fds, size_t num_sockets) {

	struct subscriber_message subscriber_command, message;
	
	memset(&subscriber_command, 0, sizeof(subscriber_command));
	memset(&message, 0, sizeof(message));

	// Read the expected data
	int rc = recv_all(poll_fds[i].fd, &subscriber_command,
									sizeof(subscriber_command));
				DIE(rc < 0, "recv");

	string new_name;

	switch(subscriber_command.sig) {
		case EXIT:

		close(poll_fds[i].fd);
		printf("Client %s disconnected.\n", clients[poll_fds[i].fd].id);
		clients.erase(poll_fds[i].fd);

		for (int j = i; j < num_sockets - 1; j++) {
			poll_fds[j] = poll_fds[j + 1];
		}

		num_sockets--;
		break;

		case SUBSCRIBE:

		// Add \0 if id strlen(topic) == 50
		char topic[MAX_TOPICSIZE + 1];
		memset(topic, 0, MAX_TOPICSIZE + 1);
		memcpy(topic, subscriber_command.topic, MAX_TOPICSIZE);

		// Add topic to topics hashmap
		new_name = clients[poll_fds[i].fd].id;
		topics[new_name].push_back(topic);

		// Notify the client of the successful subscription
		message.sig = CONFIRM;
		rc = send_all(poll_fds[i].fd, &message, sizeof(subscriber_message));
		DIE(rc < 0, "Send");

		break;

		case UNSUBSCRIBE:

		new_name = clients[poll_fds[i].fd].id;

		// Delete the topic from the client's list
		for (size_t i = 0; i < topics[new_name].size();i++)
			if (topics[new_name][i] == std::string(subscriber_command.topic)) {
				topics[new_name].erase(topics[new_name].begin() + i);	
			}

		// Notify the client of the successful unsubscription
		message.sig = CONFIRM;
		rc = send_all(poll_fds[i].fd, &message, sizeof(subscriber_message));
		DIE(rc < 0, "Send");
		break;
	}
}
/**
 * @brief The server's functionality
 * 
 * @param tcp_fd for managing tcp connections
 * @param udp_fd for managing udp connections
 */
void run_server(uint32_t tcp_fd, uint32_t udp_fd) {

	// Allow as many clients as possible with resizeable array
	size_t poll_size = MAX_CONNECTIONS;
	struct pollfd* poll_fds =
		(struct pollfd*)malloc(MAX_CONNECTIONS * sizeof(struct pollfd));
	DIE(!poll_fds, "malloc");

  	size_t num_sockets = 1;

	struct client current_client;
  	struct id_packet received_packet;
	struct server_message udp;
	struct sockaddr_in cli_addr;
	socklen_t cli_len = sizeof(cli_addr);
	char buf[MSG_MAXSIZE + 1];
	int rc;

	memset(&udp, 0, sizeof(udp));

  	// Set the socket for TCP in LISTEN
  	rc = listen(tcp_fd, MAX_CONNECTIONS);
  	DIE(rc < 0, "listen");

 	// The server can receive data from stdin or a TCP/UDP client
  	poll_fds[0].fd = tcp_fd;
  	poll_fds[0].events = POLLIN;

	num_sockets++;
	poll_fds[1].fd = fileno(stdin);
	poll_fds[1].events = POLLIN;

	num_sockets++;
	poll_fds[2].fd = udp_fd;
	poll_fds[2].events = POLLIN;

	while (1) {
		// Wait for an event
		rc = poll(poll_fds, num_sockets, -1);
		DIE(rc < 0, "poll");

		// Check for any updates from the sockets
		for (size_t i = 0; i < num_sockets; i++) {
			if (poll_fds[i].revents & POLLIN) {
				if (poll_fds[i].fd == tcp_fd) { // New tcp client
				
					// Accept the connection request
					struct sockaddr_in cli_addr;
					socklen_t cli_len = sizeof(cli_addr);
					const int newsockfd =
					accept(tcp_fd, (struct sockaddr *)&cli_addr, &cli_len);
					DIE(newsockfd < 0, "accept");

					//Receive the client ID
					recv_all(newsockfd, &received_packet, sizeof(received_packet));
					DIE(rc < 0, "recv");

					strcpy(current_client.id, received_packet.message);
					current_client.ip_add = cli_addr.sin_addr;
					current_client.port = ntohs(cli_addr.sin_port);

					// If the client already connected
					if (find_value(current_client.id) >= 0) {
						
						printf("Client %s already connected.\n", current_client.id);
						
						// Notify the client to close the socket by sending a
						// NULL ip_add and port
						cli_addr.sin_port = 0;
						cli_addr.sin_addr.s_addr = 0;

						rc = send_all(newsockfd, &cli_addr, sizeof(cli_addr));
							DIE(rc < 0, "Send");
						close(newsockfd);
						break;
					}
					// Extend the socket array if necessary
					if (num_sockets == poll_size)
						poll_fds = resize(poll_fds, &poll_size);

					// Add the new socket to the array
					poll_fds[num_sockets].fd = newsockfd;
					poll_fds[num_sockets].events = POLLIN;
					num_sockets++;

					// Add client information to hashmap
					memcpy(&clients[newsockfd], &current_client, sizeof(struct client));

					printf("New client %s connected from %s:%d.\n",
							(char*)clients[newsockfd].id, inet_ntoa(clients[newsockfd].ip_add),
								 ntohs(clients[newsockfd].port));
					break;
				} else if (poll_fds[i].fd == fileno(stdin)) { // Data from stdin
					cin.getline(buf, sizeof(buf));

					if (!strcmp(buf, "exit")) {
						disconnect_server(poll_fds, num_sockets);
						return;
					} else {
						printf("Invalid command\n");
						break;
					}
				} else if (poll_fds[i].fd == udp_fd) { // Message from udp client
					memset(&udp, 0, sizeof(udp));
			
					rc = recvfrom(poll_fds[i].fd, &udp, sizeof(struct server_message),
						0, (struct sockaddr *)&cli_addr, &cli_len);
					DIE(!rc, "recv failed");
					
					// Notify the clients which have a subscription for topic
					for (auto it: clients) {
						string name = it.second.id;

						for (size_t i = 0; i < topics[name].size(); i++) {
							// Topic can have exactly 50 characters, so i extend it
							char topic[MAX_TOPICSIZE + 1];
							memset(topic, 0, MAX_TOPICSIZE + 1);
							memcpy(topic, udp.topic,MAX_TOPICSIZE);

							if (is_match(std::string(topic), topics[name][i])) {
								// Send ip, port and data to client
							 	int new_fd = it.first;
								rc = send_all(new_fd, &cli_addr, sizeof(cli_addr));
								DIE(rc < 0, "Send");

								rc = send_all(new_fd, &udp, sizeof(udp));
								DIE(rc < 0, "Send");
								break;
							}
						}
					}
				} else { 
					// A tcp client sent a command
					tcp_communication(i, poll_fds, num_sockets);
				}
			}
		}
  }
}

int main(int argc, char *argv[]) {

	// Disable display buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc != 2) {
		printf("\nUsage: %s <port>\n", argv[0]);
		return 1;
	}

	// Parse port as a number
	uint16_t port;
	int rc = sscanf(argv[1], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

	// Get a TCP socket for managing TCP connections
	const int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_fd < 0, "socket");

	// Get an UDP socket for managing UDP connections
	const int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_fd < 0, "socket");

	// Fill in serv_address the server's address, port and family
	struct sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(struct sockaddr_in);

	// Make the socket address reusable
	const int enable = 1;
	rc = setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
	DIE(rc < 0, "setsockopt");
	
	rc = setsockopt(tcp_fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
	DIE(rc < 0, "setsockopt");

	memset(&serv_addr, 0, socket_len);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	rc = inet_pton(AF_INET, DEFAULT_IP, &serv_addr.sin_addr.s_addr);
	DIE(rc <= 0, "inet_pton");

	// Bind the server address with the TCP socket
	rc = bind(tcp_fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "bind");

	// Bind the server address with the UDP socket
	rc = bind(udp_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "bind");

	// Let the magic happen
	run_server(tcp_fd, udp_fd);

	return 0;
}
