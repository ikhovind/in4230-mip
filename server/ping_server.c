//
// Created by ikhovind on 22.09.24.
//

#include <stdio.h>        /* printf */
#include <stdlib.h>       /* free */
#include <unistd.h>       /* fgets */
#include <string.h>       /* memset */
#include <sys/socket.h>   /* socket */

#include "../network_interface/network_util.h"
#define MAX_EVENTS 10
#define BUF_SIZE 1450
static uint8_t dst_addr[6];


#include <sys/un.h>

#define BUFFER_SIZE 1024

int main(int argc, char** argv) {

	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s "
				       "[-h] "
				       "<socket_lower>", argv[0]);
				exit(0);
		}
	}

	uint8_t pos_arg_start = 1;
	char* socket_lower = argv[pos_arg_start];


	int client_fd;
	struct sockaddr_un server_addr;
	char buffer[BUFFER_SIZE] = "Hello, Server!";
	char msg_buffer[BUFFER_SIZE];

	client_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (client_fd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Set up the socket address structure
	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, socket_lower, sizeof(server_addr.sun_path) - 1);

	// Connect to the server
	if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) == -1) {
		perror("connect");
		close(client_fd);
		exit(EXIT_FAILURE);
	}

	// Clear buffer and receive echoed data from the server
	while(1) {
		memset(buffer, 0, BUFFER_SIZE);
		ssize_t num_read = read(client_fd, buffer, BUFFER_SIZE);
		if (num_read > 0) {
			mip_ping_sdu* ping_sdu = deserialize_mip_ping_sdu(buffer);
			printf("Received: %s\n", ping_sdu->message);
			strcpy(msg_buffer, "PONG:");
			strcat(msg_buffer, ping_sdu->message);
			ping_sdu->message = msg_buffer;
			size_t ping_sdu_size = 0;
			uint8_t* serial_ping_sdu = serialize_mip_ping_sdu(ping_sdu, &ping_sdu_size);
			printf("msg_buffer: %s\n", msg_buffer);
			printf("msg: %s\n", ping_sdu->message);
			write(client_fd, serial_ping_sdu, ping_sdu_size);
		} else if (num_read == -1) {
			perror("read");
		}
	}

	// Close the connection
	close(client_fd);

	return 0;
}
