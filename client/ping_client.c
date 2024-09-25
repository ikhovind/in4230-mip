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
#include <ifaddrs.h>	/* getifaddrs */

#include "../network_interface/network_util.h"
#include "../chat.h"


#define MAX_EVENTS 10
#define BUF_SIZE 1450
#define DST_MAC_ADDR {0x00, 0x00, 0x00, 0x00, 0x00, 0x02}


int main(int argc, char** argv)
{
	int opt; 
	
	printf("%d\n", argc);

	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s "
				       "[-s] server mode "
				       "[-c] client mode\n", argv[0]);
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

	send_message(socket_lower, &ping_sdu);
	exit(0);
}
