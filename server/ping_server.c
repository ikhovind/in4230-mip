
#include <stdint.h>
#include <stdio.h>        /* printf */
#include <stdlib.h>       /* free */
#include <unistd.h>       /* fgets */
#include <string.h>       /* memset */
#include <bits/getopt_core.h>
#include <sys/socket.h>   /* socket */

#include "../packet_builder/mip_builder.h"

#include "../network_interface/network_util.h"
#define MAX_EVENTS 10
#define BUF_SIZE 1450
static uint8_t dst_addr[6];
#include <sys/un.h>

#include "../packet_builder/mip_builder.h"

#define sun_path_size 108

/**
 * Program entry point, responds to  messages received from the server
 */
int main(int argc, char** argv) {

	// include null terminated character and the abstract namespace character
	char sock_path_buf[sun_path_size];
	sock_path_buf[0] = '\0';
	int opt;

	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s "
				       "[-h] "
				       "<socket_lower>\n", argv[0]);
				exit(0);
		}
	}

	if (argc < 2)
	{
		printf("Too few arguments, <socket_lower> is mandatory: ./ping_server [-h] <socket_lower>\n");
		exit(EXIT_FAILURE);
	}

	uint8_t pos_arg_start = 1;
	strncpy(sock_path_buf + 1, argv[pos_arg_start], strlen(argv[pos_arg_start]));


	int client_fd;
	struct sockaddr_un server_addr;
	// len without mip address
	char msg_buffer[MIP_PDU_MAX_SIZE - 1];
	// whole SDU
	char sdu_buffer[MIP_PDU_MAX_SIZE];

	client_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (client_fd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Set up the socket address structure
	memset(&server_addr, 0, sizeof(struct sockaddr_un));
	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, sock_path_buf, sizeof(server_addr.sun_path));

	// Connect to the server
	if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) == -1) {
		perror("connect");
		close(client_fd);
		exit(EXIT_FAILURE);
	}

	// Clear buffer and receive echoed data from the server
	while(1) {
		ssize_t num_read = read(client_fd, sdu_buffer, MIP_PDU_MAX_SIZE);
		if (num_read > 0) {
			mip_ping_sdu ping_sdu;
			deserialize_mip_ping_sdu(&ping_sdu, sdu_buffer);
			print_mip_ping_sdu(&ping_sdu, 4);
			// check it starts with PING
			if (strncmp("PING:", ping_sdu.message, 5) != 0) {
				printf("Received message does not start with PING:, ignoring\n");
				continue;
			}
			printf("Received: %s\n", ping_sdu.message);
			// ping -> pong
			ping_sdu.message[1] = 'O';
			size_t padded_length = serialize_mip_ping_sdu(sdu_buffer, &ping_sdu);
			printf("msg: %s\n", ping_sdu.message);
			write(client_fd, sdu_buffer, padded_length);
			free(ping_sdu.message);
		} else if (num_read == -1) {
			perror("read");
		}
	}
}
