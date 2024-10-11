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
#include <bits/getopt_core.h>

#include <sys/un.h>	/* definitions for UNIX domain sockets */
#include <sys/time.h>

#include "../network_interface/network_util.h"


#define MAX_EVENTS 10
#define DST_MAC_ADDR {0x00, 0x00, 0x00, 0x00, 0x00, 0x02}
uint8_t mip_sdu_buf[MIP_SDU_MAX_LENGTH];


int main(int argc, char** argv)
{
	struct sockaddr_un addr;
	int	   opt, sd, rc;

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

	// reserve space for abstract namespace character
	char sock_path_buf[sizeof(addr.sun_path) - 1];
	sock_path_buf[0] = '\0';

	uint8_t pos_arg_start = 1;
	strncpy(sock_path_buf + 1, argv[pos_arg_start], strlen(argv[pos_arg_start]));
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


	sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock_path_buf, sizeof(addr.sun_path) - 2);

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

	serialize_mip_ping_sdu(mip_sdu_buf, &ping_sdu);
	printf("Writing to server\n");
	// start counter to measrure RTT
	struct timeval start;
	gettimeofday(&start, NULL);
	rc = write(sd, mip_sdu_buf, sizeof(uint8_t) + strlen(ping_sdu.message) + 1);

	if (rc < 0) {
		perror("write");
		close(sd);
		exit(EXIT_FAILURE);
	}
	// Receive message
	ssize_t bytes_received = recv(sd, mip_sdu_buf, sizeof(mip_sdu_buf), 0);

	if (bytes_received == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			fprintf(stderr, "Timeout\n");
		} else {
			perror("recv");
		}
	} else if (bytes_received > 0) {
		// print RTT
		struct timeval end;
		gettimeofday(&end, NULL);
		mip_ping_sdu rec_ping_sdu;
		// check if message matches what we sent
		deserialize_mip_ping_sdu(&rec_ping_sdu, mip_sdu_buf);

		snprintf(mip_sdu_buf, sizeof(mip_sdu_buf), "PONG:%s", message);
		if (strcmp(rec_ping_sdu.message, mip_sdu_buf) == 0) {
			printf("RTT: %ld ms\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000);
			print_mip_ping_sdu(&rec_ping_sdu, 2);
			free(rec_ping_sdu.message);
		}
	}

	exit(0);
}
