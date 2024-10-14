#include <stdint.h>
#include <stdlib.h>	/* free */
#include <stdio.h>	/* printf */
#include <unistd.h>	/* fgets */
#include <string.h>	/* memset */
#include <sys/socket.h>	/* socket */
#include <sys/epoll.h>		/* epoll */
#include <arpa/inet.h>	/* htons */
#include <errno.h>
#include <bits/getopt_core.h>

#include <sys/un.h>	/* definitions for UNIX domain sockets */
#include <sys/time.h>

#include "../packet_builder/mip_builder.h"

uint8_t mip_sdu_buf[MIP_SDU_MAX_LENGTH];


/**
 * Program entry point, sends a message to the server and waits for a response according to user
 */
int main(int argc, char** argv)
{
	struct sockaddr_un addr;
	int	   opt, sd, rc;

	char msg_buffer[MIP_PDU_MAX_SIZE - 1] = "PING:";
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s "
				       "[-h] "
				       "<socket_lower> "
				       "<mip destination address> "
				       "<message>\n", argv[0]);
				exit(0);
		}
	}

	if (argc < 4)
	{
		printf("Too few arguments, socket_lower, destination_host and message are mandatory: ./ping_client [-h] <socket_lower> <destination host> <message>\n");
		exit(EXIT_FAILURE);
	}



	// reserve space for abstract namespace character
	char sock_path_buf[sizeof(addr.sun_path) - 1];
	sock_path_buf[0] = '\0';

	uint8_t pos_arg_start = 1;
	strncpy(sock_path_buf + 1, argv[pos_arg_start], strlen(argv[pos_arg_start]));

	// parse dest host using strtol
	const uint8_t dest_host = strtol(argv[pos_arg_start + 1], NULL, 10);
	char* message = argv[pos_arg_start + 2];
	strcat(msg_buffer, message);

	mip_ping_sdu ping_sdu = {
		.mip_address = dest_host,
		.message = msg_buffer
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

	size_t padded_length = serialize_mip_ping_sdu(mip_sdu_buf, &ping_sdu);
	printf("Writing \"%s\" to server: %d\n", msg_buffer, dest_host);
	// start counter to measrure RTT
	rc = write(sd, mip_sdu_buf, padded_length);
	if (rc < 0) {
		perror("write");
		close(sd);
		exit(EXIT_FAILURE);
	}

	struct timeval start;
	gettimeofday(&start, NULL);

	if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
		perror("setsockopt");
		close(sd);
		exit(EXIT_FAILURE);
	}

	// Receive message
	ssize_t bytes_received = recv(sd, mip_sdu_buf, sizeof(mip_sdu_buf), 0);

	// check error from recv
	if (bytes_received == 0) {
		fprintf(stderr, "Connection closed\n");
	}
	if (bytes_received == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			fprintf(stderr, "Timeout\n");
			// print time
			struct timeval end;
			gettimeofday(&end, NULL);
			printf("RTT: %ld ms\n", (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000);
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
		}
		else {
			printf("String did not match\n");
			print_mip_ping_sdu(&rec_ping_sdu, 4);
		}
		free(rec_ping_sdu.message);
	}
	printf("exiting\n");
	exit(0);
}
