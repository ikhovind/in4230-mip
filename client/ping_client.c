//
// Created by ikhovind on 22.09.24.
//

#include <stdint.h>
#include <stdlib.h>	/* free */
#include <stdio.h>	/* printf */
#include <unistd.h>	/* fgets */
#include <string.h>	/* memset */
#include <sys/socket.h>	/* socket */
#include <fcntl.h>
#include <sys/epoll.h>		/* epoll */
#include <linux/if_packet.h>	/* AF_PACKET */
#include <net/ethernet.h>	/* ETH_* */
#include <arpa/inet.h>	/* htons */
#include <errno.h>

#include <sys/un.h>	/* definitions for UNIX domain sockets */
#include <sys/time.h>

#include "../network_interface/network_util.h"


#define MAX_EVENTS 10
#define BUF_SIZE 1450
#define DST_MAC_ADDR {0x00, 0x00, 0x00, 0x00, 0x00, 0x02}


int main(int argc, char** argv)
{
	int opt; 
	
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s "
				       "[-h] "
				       "<socket_lower> "
				       "<mip destination address>"
				       "<message>", argv[0]);
				exit(0);
		}
	}

	if (argc < 3)
	{
		printf("Too few arguments, socket_lower, destination_host and message are mandatory: ./ping_client [-h] <socket_lower> <destination host> <message>\n");
		exit(EXIT_FAILURE);
	}

	uint8_t pos_arg_start = 1;
	char* socket_lower = argv[pos_arg_start];
	uint8_t dest_host = atoi(argv[pos_arg_start + 1]);
	char* message = argv[pos_arg_start + 2];

	mip_ping_sdu ping_sdu = {
		.mip_address = dest_host,
		.message = message
	};
	struct timeval timeout;
	timeout.tv_sec = 1; // Set timeout to 1 second
	timeout.tv_usec = 0;

	struct sockaddr_un addr;
	int	   sd, rc;
	char   buf[256];

	sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_lower, sizeof(addr.sun_path) - 1);

	rc = connect(sd, (struct sockaddr *)&addr, sizeof(addr));
	if ( rc < 0) {
		perror("connect");
		close(sd);
		exit(EXIT_FAILURE);
	}

	if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
		perror("setsockopt");
		close(sd);
		exit(EXIT_FAILURE);
	}

	size_t mip_ping_sdu_size = sizeof(ping_sdu) + strlen(ping_sdu.message);

	size_t serialized_size = 0;
	uint8_t* serialized = serialize_mip_ping_sdu(&ping_sdu, &serialized_size);
	rc = write(sd, serialized, serialized_size);

	free(serialized);

	if (rc < 0) {
		perror("write");
		close(sd);
		exit(EXIT_FAILURE);
	}
	memset(buf, 0, 256);

	// Receive message
	ssize_t bytes_received = recv(sd, buf, sizeof(buf) - 1, 0);

	if (bytes_received == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			fprintf(stderr, "Timeout\n");
		} else {
			perror("recv");
		}
	} else if (bytes_received > 0) {
		mip_ping_sdu* ping_sdu = deserialize_mip_ping_sdu(buf);
		printf("Received:\n");
		print_mip_ping_sdu(ping_sdu);
	}

	exit(0);
}
