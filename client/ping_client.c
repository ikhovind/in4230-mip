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


/*
 * Print MAC address
 */
void print_mac_addr(uint8_t *addr, size_t len)
{
	size_t i;

	for (i = 0; i < len - 1; i++) {
		printf("%d:", addr[i]);
	}
	printf("%d", addr[i]);
}

/*
 * Receive packet from the RAW socket
 */

/*
 * This function gets a pointer to a struct sockaddr_ll
 * and fills it with necessary address info from the interface device.
 */


int raw_socket(int argc, char *argv[])
{
	int	raw_sock, rc;
	struct	sockaddr_ll so_name;
	uint8_t buf[BUF_SIZE];
	char	username[16];

	short unsigned int protocol;
	protocol = 0xFFFF;

	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd;

	/* Set up a raw AF_PACKET socket without ethertype filtering */
	raw_sock = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if (raw_sock == -1) {
		perror("socket");
		return -1;
	}

	get_mac_from_interface(&so_name);

	printf("\nEnter your username: \n");

	fgets(username, sizeof(username), stdin);

	/* fgets() stores the [Enter] character in the username;
	 * Replace that with '\0' so that we have a proper string.
	 * Note that strlen returns the length of the string excluding
	 * the null-terminated character
	 */

	username[strlen(username) - 1] = '\0';

	/* Send the username over the RAW socket. */
	send_raw_packet(raw_sock,
			&so_name,
			(uint8_t *)username,
			sizeof(username));

	fflush(stdin);

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = raw_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, raw_sock, &ev) == -1) {
		perror("epoll_ctl: raw_sock");
		exit(EXIT_FAILURE);
	}

	ev.data.fd = 0;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, 0, &ev) == -1) {
		perror("epoll_ctl: stdin");
		exit(EXIT_FAILURE);
	}

	while(1) {
		rc = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (rc == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		if (events->data.fd == 0) {
			printf("\n(Someone knocked on stdin...)\n");
			memset(buf, 0, BUF_SIZE);
			fgets((char *)buf, sizeof(buf), stdin);
			if (strstr((char *)buf, "ADIOS") != NULL) {
				printf("ADIOS\n");
				break;
			}

			/* Send the user message over the RAW socket */
			send_raw_packet(raw_sock,
					&so_name,
					buf,
					strlen((char *)buf));
		} else if (events->data.fd == raw_sock) {
			printf("\n(Someone knocked on the socket...)\n");

			memset(buf, 0, BUF_SIZE);
			rc = recv_raw_packet(raw_sock, buf, BUF_SIZE);
			if (rc < 1) {
				perror("recv");
				return 1;
			}

			printf("\n<server>: %s\n", buf);
			fflush(stdin);
		}
		else {
			printf("\n(Someone knocked on %d...)\n", events->data.fd);

		}
	}

	close(raw_sock);

	return 0;
}

int main(int argc, char** argv)
{
	int opt; 
	
	printf("%d\n", argc);
	if (argc < 3)
	{
		printf("Too few arguments, socket_lower, destination_host and message are mandatory: ./ping_client [-h] <socket_lower> <destination host> <message>\n");
		exit(EXIT_FAILURE);
	}
	
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
			case 'h':
				printf("Usage: %s "
				       "[-s] server mode "
				       "[-c] client mode\n", argv[0]);
				exit(0);
		}
	}
	uint8_t pos_arg_start = 1;
	char* socket_lower = argv[pos_arg_start];
	client(socket_lower);
}
