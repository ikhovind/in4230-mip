//
// Created by ikhovind on 22.09.24.
//

#include <stdio.h>        /* printf */
#include <stdlib.h>       /* free */
#include <unistd.h>       /* fgets */
#include <string.h>       /* memset */
#include <sys/socket.h>   /* socket */
#include <sys/epoll.h>    /* epoll */
#include <linux/if_packet.h> /* AF_PACKET */
#include <arpa/inet.h>    /* htons */

#include "../network_interface/network_util.h"
#define MAX_EVENTS 10
#define BUF_SIZE 1450
static uint8_t dst_addr[6];


int main(int argc, char *argv[])
{
	struct	  sockaddr_ll so_name;
	uint8_t	  buf[BUF_SIZE];
	uint8_t	  out_buf[BUF_SIZE];
	uint8_t	  username[16];
	int	  raw_sock, rc;

	struct epoll_event ev, events[MAX_EVENTS];
	int epollfd;

	short unsigned int protocol = 0xFFFF;

	/* Set up a raw AF_PACKET socket without ethertype filtering */
	raw_sock = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if (raw_sock == -1) {
		perror("socket");
		return -1;
	}

	printf("\n(Waiting for sender to connect...)\n\n");
	rc = recv_raw_packet(raw_sock, username, sizeof(username));
	if (rc == -1) {
		perror("recv_raw_packet");
		return -1;
	}

	printf("%s knocked on the door...\n", username);

	/* Fill the fields of so_name with info from interface */
	get_mac_from_interface(&so_name);

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

		memset(buf, 0, BUF_SIZE);
		memset(out_buf, 0, BUF_SIZE);

		if (events->data.fd == raw_sock) {
			printf("Someone knocked on the raw socket...\n");
			rc = recv_raw_packet(raw_sock, buf, BUF_SIZE);
			sprintf(out_buf, "PONG: %s", buf);
			if (rc < 1) {
				perror("recv");
				return -1;
			}
			printf("\nReceived: %s\n", buf);

			/* Send the received message over the RAW socket */
			send_raw_packet(raw_sock,
					&so_name,
					//buf,
					out_buf,
					strlen((const char *)out_buf));
					//strlen((const char *)buf));
		}
	}

	close(raw_sock);

	return 0;
}
