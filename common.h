#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <bits/stdc++.h>
 
using namespace std;

/* constants*/
#define MSG_MAXSIZE 1024
#define MAX_IDSIZE 10
#define EXIT 0
#define SUBSCRIBE 1
#define UNSUBSCRIBE 2
#define CONFIRM 3
#define MAX_CONNECTIONS 512
#define MAX_TOPICSIZE 50
#define MAX_DATA_SIZE 1500
#define MAX_IP_ADD 4294967295
#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3
#define DEFAULT_IP "127.0.0.1"

bool is_match(string st, string pattern);
int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

// Used for sending the tcp client's ID over the network
struct id_packet {
	uint16_t len;
	char message[MAX_IDSIZE];
};

struct client {
	char id[MAX_IDSIZE + 1];
	uint16_t port;
	in_addr ip_add;
};

struct __attribute__ ((__packed__)) subscriber_message {
	uint8_t sig;
	char topic[MAX_TOPICSIZE];
};

struct __attribute__ ((__packed__)) server_message {
	char topic[MAX_TOPICSIZE];
	uint8_t data_type;
	uint8_t data[MAX_DATA_SIZE];
};

#endif
