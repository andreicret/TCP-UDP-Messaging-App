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

using namespace std;

const char sep[]= " \n";

/**
 * @brief Prints the message received from an udp client according to
 * the data type specified in udp_message.data_type
 * 
 * @param udp_message the message received from the udp client
 * @param cli_addr the ip address and port of the udp client
 */
void parse_message(struct server_message udp_message, struct sockaddr_in cli_addr) {

	printf("%s:%d - ", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	printf("%s - ", udp_message.topic);

	switch (udp_message.data_type) {
		case INT:
		{
			uint32_t num = *(uint32_t*)((uint8_t*)udp_message.data + 1);
			printf("INT - ");

			if (*(uint8_t*) udp_message.data == 1 && ntohl(num) != 0)
				printf("-");

			printf("%d\n", ntohl(num));
			break;
		}
		case SHORT_REAL:
		{
			printf("SHORT_REAL - ");
			uint16_t num = ntohs(*(uint16_t*)udp_message.data);
			
			printf("%.2f\n", (double)num / 100.0);
			break;
		}
		case FLOAT:
		{
			printf("FLOAT - ");
			if (*(uint8_t*) udp_message.data == 1)
			printf("-");

			uint32_t num = *(uint32_t*)((uint8_t*)udp_message.data + 1);
			num = ntohl(num);

			//The abs of the power is the fifth byte from the payload
			uint8_t abs_power = *(uint8_t*)(udp_message.data + 5);
			uint32_t power = 1;

			//Raise to the correct power to represent the number correctly
			for (size_t i = 0; i < abs_power; i++)
				power *= 10;
			
			cout<<fixed<<setprecision(abs_power)<<(float)num / power<<"\n";
			break;
		}
		case STRING:
		{
			printf("STRING - ");
			printf("%s\n", udp_message.data);
			break;
		}	
	}
}

/**
 * @brief Separate the input given from keyboard into <command, topic>
 * 
 * @param buf the input from stdin
 * @param command command - subscribe/unsubscribe/exit
 * @param topic the name of the topic relevant for the app
 */
void get_message(char* buf, char command[MSG_MAXSIZE + 1], char topic[MSG_MAXSIZE + 1]) {

	char *p = strtok(buf, sep);
	strcpy(command, p);
	p = strtok(NULL, sep);

	// If exit command is given, p is NULL
	if (p)
		strcpy(topic, p);
}

/**
 * @brief The client's functionality
 * 
 * @param sockfd socket which facilitates the connection to server
 * @param subscriberID client's id
 */
void run_subscriber(int sockfd, char* subscriberID) {

	char buf[MSG_MAXSIZE + 1];
	char aux[MSG_MAXSIZE + 1];
	char command[MSG_MAXSIZE + 1];
	char topic[MSG_MAXSIZE + 1];
	memset(buf, 0, MSG_MAXSIZE + 1);

	struct id_packet sent_packet;
	struct subscriber_message message;

	struct pollfd poll_fds[MAX_CONNECTIONS];
	int num_sockets = 2, rc;

	memset(&sent_packet, 0, sizeof(sent_packet));
	memset(&message, 0, sizeof(subscriber_message));
	// Send the subscriberID to the server
	sent_packet.len = strlen(subscriberID) + 1;
	strcpy(sent_packet.message, subscriberID);

	rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
	DIE(rc < 0, "send");
	
	// Client can receive data from either stdin or server
	poll_fds[0].fd = fileno(stdin);
	poll_fds[0].events = POLLIN;

	poll_fds[1].fd = sockfd;
	poll_fds[1].events = POLLIN | POLLOUT;

	while (1) {
		// Wait for an event
		rc = poll(poll_fds, num_sockets, -1);
		DIE(rc < 0, "poll");

		// Data from stdin
		if ((poll_fds[0].revents & POLLIN) != 0) {

			cin.getline(buf, MSG_MAXSIZE + 1);
			strcpy(aux, buf);
			get_message(aux, command, topic);

			if (!strcmp(command, "exit")) {
				// Notify the server that the client disconnects
				message.sig = EXIT;
				rc = send_all(sockfd, &message, sizeof(message));
				DIE(rc < 0, "Send");

				close(sockfd);
				exit(0);
				
			} else if (!strcmp(command, "subscribe")) {
				message.sig = SUBSCRIBE;
				strcpy(message.topic, topic);
				
				// Send the subscription request to the server
				send_all(sockfd, &message, sizeof(message));
				DIE(rc < 0, "Send");
				
				rc = recv_all(poll_fds[1].fd, &message,
					sizeof(struct subscriber_message));
				DIE(rc <0, "recv");

				// The server completed the action successfully
				if (message.sig == CONFIRM)
					printf("Subscribed to topic %s\n", topic);
				else
					exit(-1);
				continue;

			} else if (!strcmp(command, "unsubscribe")) {
				// Send the unsubscription request to the server
				message.sig = UNSUBSCRIBE;
				strcpy(message.topic, topic);

				send_all(sockfd, &message, sizeof(message));
				DIE(rc < 0, "Send");

				rc = recv_all(poll_fds[1].fd, &message,
					sizeof(struct subscriber_message));
				DIE(rc <0, "recv");

				if (message.sig == CONFIRM)
					printf("Unsubscribed from topic %s\n", topic);
				else
					exit(-1);
				continue;
			}

		} else if ((poll_fds[1].revents & POLLIN) != 0) {

			// Message from an udp client
			struct server_message udp_message;
			struct sockaddr_in cli_addr;

			// The udp client's information is received first
			rc = recv_all(poll_fds[1].fd, &cli_addr, sizeof(cli_addr));
			DIE(rc < 0, "recv");
			
			// If the ip_address and port are 0, the server has given the
			// signal that one should close the socket and exit the program
			if (cli_addr.sin_port == 0 && cli_addr.sin_addr.s_addr == 0) {
				close(sockfd);
				exit(0);
			}

			// Receive the expected data
			rc = recv_all(poll_fds[1].fd, &udp_message, sizeof(udp_message));
			DIE(rc < 0, "recv");

			// Print the data accordingly
			parse_message(udp_message, cli_addr);
		}
	}
}

int main(int argc, char *argv[]) {

	// Disable display buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	if (argc != 4) {
		printf("\n Usage: %s <subscriberID> <ip> <port>\n", argv[0]);
		return 1;
	}
	 
	// Take the subscriberID
	char *subscriberID = argv[1];

	// Parse port as a number
	uint16_t port;
	int rc = sscanf(argv[3], "%hu", &port);
	DIE(rc != 1, "Given port is invalid");

	// Get a TCP socket for communication with server
	const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// Fill in serv_address the server's address, port and family
	struct sockaddr_in serv_addr;
	socklen_t socket_len = sizeof(struct sockaddr_in);

	memset(&serv_addr, 0, socket_len);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
	DIE(rc <= 0, "inet_pton");

	// Connect to the server
	rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "connect");

	// Let the magic happen
	run_subscriber(sockfd, subscriberID);
	return 0;
}
