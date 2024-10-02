//
// Created by ikhovind on 24.09.24.
//

// basic unix socket implementation

#define _DEFAULT_SOURCE
#include <stdio.h>	/* standard input/output library functions */
#include <stdlib.h>	/* standard library definitions (macros) */
#include <unistd.h>	/* standard symbolic constants and types */
#include <stdbool.h>
#include <string.h>	/* string operations (strncpy, memset..) */

#include <net/if.h>
#include <sys/epoll.h>	/* epoll */
#include <sys/socket.h>	/* sockets operations */
#include <sys/un.h>	/* definitions for UNIX domain sockets */
#include <stdint.h>
#include <linux/if_packet.h>	/* AF_PACKET */
#include <net/ethernet.h>	/* ETH_* */
#include <arpa/inet.h>	/* htons */
#include <signal.h>
#include <bits/sigaction.h>

#include <netinet/in.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>   // for ETH_FRAME_LEN


#include "mip_daemon.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if.h>

#include "../packet_builder/mip_builder.h"
#include "../packet_builder/eth_builder.h"
#include "../cache/arp_cache.h"
#include "../network_interface/network_util.h"

static uint8_t mip_address;
static arp_cache cache;
char socket_name[256];

int unix_sd;
int raw_sd;
bool debug = false;
struct sockaddr_ll our_addr;

// Clean up function to close the socket
void cleanup(int signum) {
	if (unix_sd != -1) {
		close (unix_sd);
		printf("Unix socket closed due to signal %d\n", signum);
	}
	if (raw_sd != -1) {
		close(raw_sd);
		printf("Raw socket closed due to signal %d\n", signum);
	}
	exit(signum);
}


/*
 * This function gets a pointer to a struct sockaddr_ll
 * and fills it with necessary address info from the interface device.
 */
int raw_socket()
{
	int	raw_sock;

	short unsigned int protocol;
	protocol = 0x88B5;

	/* Set up a raw AF_PACKET socket without ethertype filtering */
	raw_sock = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if (raw_sock == -1) {
		perror("socket");
		return -1;
	}

	return raw_sock;
}

/**
 * Prepare (create, bind, listen) the server listening socket
 *
 * Returns the file descriptor of the server socket.
 *
 * In C, you declare a function as static when you want to limit its scope to
 * the current source file. When a function is declared as static, it can only
 * be called from within the same source file where it is defined.
 */
static int prepare_server_sock(char* socket_upper)
{
	int sd = -1, rc = -1;
	struct sockaddr_un addr;

	/* 1. Create local UNIX socket of type SOCK_SEQPACKET. */

	/* Why SOCK_SEQPACKET?
	 * SOCK_SEQPACKET is a socket type that provides a reliable,
	 * connection-oriented, and sequenced data transmission mechanism.
	 *
	 * Reliability: SOCK_SEQPACKET sockets offer a reliable data
	 * transmission mechanism. It ensures that data sent from one end will
	 * be received in the same order by the other end.
	 *
	 * Connection-Oriented: SOCK_SEQPACKET sockets are connection-oriented.
	 * This means they establish a connection between two endpoints
	 * (typically through a three-way handshake) before data transmission
	 * begins. This connection setup helps in ensuring that data is sent and
	 * received reliably.
	 *
	 * Sequenced Data: In some scenarios, it's important that data is
	 * received in the order it was sent. For example, if you are
	 * transmitting parts of a file, you want them to be reassembled in the
	 * correct order. SOCK_SEQPACKET ensures this order.
	 *
	 * Guaranteed Message Boundaries: SOCK_SEQPACKET also maintains message
	 * boundaries. It means that if you send distinct messages, they will be
	 * received as distinct messages. This can simplify the message parsing
	 * on the receiving end.
	 */

	sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (sd	== -1) {
		perror("socket()");
		return rc;
	}

	/*
	 * For portability clear the whole structure, since some
	 * implementations have additional (nonstandard) fields in
	 * the structure.
	 */
	memset(&addr, 0, sizeof(struct sockaddr_un));

	/* 2. Bind socket to socket name. */

	addr.sun_family = AF_UNIX;

	/* Why `-1`??? Check character arrays in C!
	 * 's' 'e' 'r' 'v' 'e' 'r' '.' 's' 'o' 'c' 'k' 'e' 't' '\0'
	 * sizeof() counts the null-terminated character ('\0') as well.
	 */
	strncpy(addr.sun_path, socket_upper, sizeof(addr.sun_path) - 1);

	/* Unlink the socket so that we can reuse the program.
	 * This is bad hack! Better solution with a lock file,
	 * or signal handling.
	 * Check https://gavv.github.io/articles/unix-socket-reuse
	 */
	unlink(socket_upper);

	rc = bind(sd, (const struct sockaddr *)&addr, sizeof(addr));
	if (rc == -1) {
		perror("bind");
		close(sd);
		return rc;
	}

	/*
	 * 3. Prepare for accepting incomming connections.
	 * The backlog size is set to MAX_CONNS.
	 * So while one request is being processed other requests
	 * can be waiting.
	 */
	rc = listen(sd, MAX_CONNS);
	if (rc == -1) {
		perror("listen()");
		close(sd);
		return rc;
	}

	return sd;
}

static int add_to_epoll_table(int efd, struct epoll_event *ev, int fd)
{
	int rc = 0;

	/* EPOLLIN event: The associated file is available for read(2)
	 * operations. Check man epoll in Linux for more useful events.
	 */
	ev->events = EPOLLIN;
	ev->data.fd = fd;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, ev) == -1) {
		perror("epoll_ctl");
		rc = -1;
	}

	return rc;
}


int broadcast(mip_pdu* broadcast_pdu, int raw_sd) {
    printf("broadcasting\n");

    uint8_t* serial_pdu = malloc(sizeof(mip_header) + broadcast_pdu->header.sdu_len);
    serialize_mip_pdu(serial_pdu, broadcast_pdu);

    // Construct the Ethernet frame
	eth_header broadcast_header;

	build_eth_header(&broadcast_header, BROADCAST_MAC, our_addr.sll_addr, ETH_MIP_TYPE);
	eth_pdu broadcast_eth_pdu;
	build_eth_pdu(&broadcast_eth_pdu, &broadcast_header, broadcast_pdu);

	uint8_t frame2 [ETH_ARP_SIZE];
	serialize_eth_pdu(frame2, &broadcast_eth_pdu);

    struct sockaddr_ll addr = {0};
    addr.sll_ifindex = our_addr.sll_ifindex;
    addr.sll_halen = ETH_ALEN;
    memcpy(addr.sll_addr, BROADCAST_MAC, ETH_ALEN);

    // Send the Ethernet frame
    if (sendto(raw_sd, frame2, ETH_ARP_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Failed to send the frame");
        close(raw_sd);
        exit(EXIT_FAILURE);
    }

    printf("Broadcast packet sent successfully!\n");
    return 0;
}

static void send_mip_arp(uint8_t dest_address, uint8_t arp_type) {
	mip_arp_sdu arp_sdu;
	arp_sdu.type = arp_type;
	if (arp_type == ARP_REQUEST_TYPE) {
		// mip address to look up
		arp_sdu.mip_address = dest_address;
		mip_pdu mip_pdu;

		build_mip_pdu(&mip_pdu, &arp_sdu, mip_address, dest_address, 1, ARP_SDU_TYPE);
		printf("Sending broadcast message with pdu:\n");
		broadcast(&mip_pdu, raw_sd);
	}
	else if (arp_type == ARP_RESPONSE_TYPE) {
		// mip address that matched
		arp_sdu.mip_address = mip_address;
		mip_pdu mip_pdu;

		build_mip_pdu(&mip_pdu, &arp_sdu, mip_address, dest_address, 1, ARP_SDU_TYPE);
		printf("Sending broadcast response with pdu:\n");
		broadcast(&mip_pdu, raw_sd);
	}
	else {
		printf("Invalid ARP type\n");
		exit(EXIT_FAILURE);
	}
}

static void send_mip_arp_response(int sd, uint8_t dest_address, uint8_t* frame, struct sockaddr_ll *recv_addr) {
	mip_arp_sdu arp_sdu;
	arp_sdu.type = ARP_RESPONSE_TYPE;
	arp_sdu.mip_address = mip_address;
	mip_pdu mip_pdu;

	build_mip_pdu(&mip_pdu, &arp_sdu, mip_address, dest_address, 1, ARP_SDU_TYPE);
	printf("Sending mip arp response with pdu:\n");
	//broadcast(&mip_pdu);
	uint8_t *serial = malloc(sizeof(mip_header) + mip_pdu.header.sdu_len);
	serialize_mip_pdu(serial, &mip_pdu);

	uint8_t* buffer = malloc(14 + sizeof(mip_header) + mip_pdu.header.sdu_len);
	memcpy(buffer, frame, 14);
	memcpy(buffer + 14, serial, sizeof(mip_header) + mip_pdu.header.sdu_len);
	//get_mac_from_interface(&so_name);
	//ssize_t sent_bytes = write(sd, serial, size, 0, (const struct sockaddr *)recv_addr, sizeof(*recv_addr));

    ssize_t sent_bytes = sendto(sd, buffer, 14 + sizeof(mip_header) + mip_pdu.header.sdu_len, 0, (const struct sockaddr *)recv_addr, sizeof(*recv_addr));
	free(buffer);
	if (sent_bytes == -1) {
		perror("sendto");
		exit(EXIT_FAILURE);
	};
	printf("Attempt finsihed: %ld\n", sent_bytes);
}

static void handle_broadcast(int fd, int unix_sd)
{
	uint8_t buf[256];
	int rc;

	/* The memset() function fills the first 'sizeof(buf)' bytes
	 * of the memory area pointed to by 'buf' with the constant byte 0.
	 */
	memset(buf, 0, sizeof(buf));

    struct sockaddr_ll recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    rc = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&recv_addr, &addr_len);

	if (rc <= 0) {
		close(fd);
		exit(EXIT_FAILURE);
	}
	mip_pdu pdu;
	deserialize_mip_pdu(&pdu, buf + 14);


	uint8_t mac_dst[6];
	uint8_t mac_src[6];
	memcpy(mac_dst, buf, 6);
	memcpy(mac_src, buf + 6, 6);

	if (pdu.header.sdu_type == ARP_SDU_TYPE) {
		if (((mip_arp_sdu*)pdu.sdu)->type == ARP_RESPONSE_TYPE) {
			printf("Got arp response:\n");
			insert_cache_index(&cache, pdu.header.source_address, mac_src);
			return;
		}
	}

	if (pdu.header.sdu_type == PING_SDU_TYPE) {
		printf("received ping\n");

		mip_ping_sdu ping_sdu = {
			.mip_address = pdu.header.source_address,
			.message = ((mip_ping_sdu*) pdu.sdu)->message,
		};

		uint8_t* serial = malloc(sizeof(uint8_t) + strlen(ping_sdu.message) + 1);
		serialize_mip_ping_sdu(serial, &ping_sdu);
		size_t written_bytes = write(unix_sd, serial, sizeof(uint8_t) + strlen(ping_sdu.message) + 1);
		if (written_bytes == -1) {
			perror("write");
			exit(EXIT_FAILURE);
		}
		return;
	}

	// If broadcast then we first learn the MAC address of the sender
	insert_cache_index(&cache, pdu.header.source_address, mac_src);

	// Then we check if the sender is looking for us
	if (pdu.header.dest_address == mip_address) {
		printf("Received broadcast for us\n");

		mip_arp_sdu arp_sdu;
		arp_sdu.type = ARP_RESPONSE_TYPE;
		arp_sdu.mip_address = mip_address;
		mip_pdu arp_pdu;
		build_mip_pdu(&arp_pdu, &arp_sdu, mip_address, pdu.header.source_address, 1, ARP_SDU_TYPE);
		uint8_t frame[14];

		get_mac_from_interface(&recv_addr);

		memcpy(frame + 6, recv_addr.sll_addr, 6);

		// copy the source MAC address into the destination field
		memcpy(frame, buf + 6, 6);
		frame[12] = 0x88;
		frame[13] = 0xb5;
		send_mip_arp_response(fd, pdu.header.source_address, frame, &recv_addr);
	}
	else {
		printf("Received broadcast not for us, ignoring\n");
	}
}

void send_broadcast_for_mip(eth_pdu *eth_arp_response, uint8_t mip_dest_address, int epollfd, struct sockaddr_ll recv_addr) {
	printf("No MAC address found for MIP address %d, sending arp\n", mip_dest_address);
	send_mip_arp(mip_dest_address, ARP_REQUEST_TYPE);

	struct epoll_event events[MAX_EVENTS];
	// wait for ARP response
	int rc = epoll_wait(epollfd, events, 1, -1);
	if (rc == -1) {
		perror("epoll_wait");
		exit(EXIT_FAILURE);
	}
	uint8_t buf[50];
	socklen_t addr_len = sizeof(recv_addr);

	struct timeval tv;
	fd_set readfds;

	// Set timeout
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	// Clear the file descriptor set
	FD_ZERO(&readfds);
	// Add socket to the set
	FD_SET(raw_sd, &readfds);

	// Wait for ready descriptor or timeout
	int ret = select(raw_sd + 1, &readfds, NULL, NULL, &tv);
	if (ret == -1) {
		perror("select error");
		exit(EXIT_FAILURE);
	}
	if (ret == 0) {
		printf("recvfrom timed out.\n");
		eth_arp_response = NULL;
		return;
	}


	ret = select(raw_sd + 1, &readfds, NULL, NULL, &tv);
	if (ret == -1) {
		perror("select error");
		close(raw_sd);
		exit(EXIT_FAILURE);
	} else if (ret == 0) {
		printf("recvfrom timed out.\n");
	}



	// a. After the ARP we should read the incoming ARP response
	rc = recvfrom(raw_sd, buf, sizeof(buf), 0, (struct sockaddr *)&recv_addr, &addr_len);
	printf("rc: %d\n", rc);
	// check if recvfrom faced an error that was NOT timeout

	if (rc < 0) {
		perror("recvfrom");
		close(raw_sd);
		exit(EXIT_FAILURE);
	}
	printf("arp response received\n");
	deserialize_eth_pdu(eth_arp_response, buf);
}

void server(char* socket_upper)
{
	struct epoll_event ev, events[MAX_EVENTS];
	int	   accept_sd, epollfd, rc;
	// for communicating with nodes over unix socket
	uint8_t node_buffer[MIP_SDU_MAX_LENGTH];
	// for communicating with other mips over raw socket
	uint8_t mipd_buffer[MIP_PDU_MAX_SIZE];


	// Set up signal handler for SIGINT
	struct sigaction sa;
	sa.sa_handler = cleanup;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	get_mac_from_interface(&our_addr);

	printf("\n*** IN3230 Multiclient chat server is running! ***\n"
	       "* Waiting for nodes to connect... *\n\n");
	fflush(stdout);

	/* Call the method to create, bind and listen to the server socket */
	unix_sd = prepare_server_sock(socket_upper);
	raw_sd = raw_socket();

	/* Create the main epoll file descriptor */
	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		close(unix_sd);
		exit(EXIT_FAILURE);
	}

	/* Add the main listening socket to epoll table */
	rc = add_to_epoll_table(epollfd, &ev, unix_sd);
	if (rc == -1) {
		close(raw_sd);
		close(unix_sd);
		exit(EXIT_FAILURE);
	}
	rc = add_to_epoll_table(epollfd, &ev, raw_sd);
	if (rc == -1) {
		close(raw_sd);
		close(unix_sd);
		exit(EXIT_FAILURE);
	}



	while (1) {
		rc = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (rc == -1) {
			perror("epoll_wait");
			close(unix_sd);
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < rc; ++i) {
			if (events[i].data.fd == unix_sd) {
				/* someone is trying to connect() to the main listening
				 * socket.
				 */
				accept_sd = accept(unix_sd, NULL, NULL);
				if (accept_sd == -1) {
					perror("accept");
					continue;
				}


				printf("<%d> joined the chat...\n", accept_sd);

				/* Add the new socket to epoll table */
				rc = add_to_epoll_table(epollfd, &ev, accept_sd);
				if (rc == -1) {
					close(unix_sd);
					exit(EXIT_FAILURE);
				}
			} else if (events[i].data.fd == raw_sd) {
				/* We have received a packet on the raw socket
				 *
				 * This means that it was a broadcast or a ping forwarded by a mipd
				 *
				 * We know this will be a mip pdu
				 */
				printf("Received data on the raw socket from: %d\n", events[i].data.fd);
				printf("accept_sd: %d\n", accept_sd);
				handle_broadcast(events[i].data.fd, accept_sd);
				/*
				 * We should:
				 *  1. Read the data
				 *  2. Deserialize the packet
				 *  3. Check if it is a broadcast or a ping
				 *     a. If it is a broadcast, we should check if it is for us
				 *        i. If it is for us
				 *			aa. We should learn the MAC address of the sender
				 *			ab. If it is an ARP request, we should respond with an ARP response
				 *         we should respond with an ARP response
				 *        ii. If it is not for us, we should ignore it
				 *     b. If it is a ping, we should forward it to the correct client
				 *
				 */
			} else {
				printf("Received data on unix socket from: %d\n", events[i].data.fd);
				/* existing node is trying to send data (ping_client or ping_server) */
				int node_fd = events[i].data.fd;

				// ############### We should: ##############3
				// 1. Read the data
				// don't need to memset since a message from the node will always be null-terminated
				int rc1 = read(node_fd, node_buffer, sizeof(node_buffer));

				if (rc1 < 0) {
					perror("read");
					close(node_fd);
					exit(EXIT_FAILURE);
				}
				if (rc1 == 0) {
					printf("Node closed communication\n");
					close(node_fd);
				}
				else {
				    // 2. Deserialize the packet
					mip_ping_sdu ping_sdu;
					deserialize_mip_ping_sdu(&ping_sdu, node_buffer);

					// 3. Build the MIP PDU
				    mip_pdu ping_pdu;
				    build_mip_pdu(&ping_pdu, &ping_sdu, mip_address, ping_sdu.mip_address, 1, PING_SDU_TYPE);

					// 4. Check the cache for the destination MIP address
				    uint8_t* dest_mac = get_mac_address(&cache, ping_pdu.header.dest_address);
					// 5. If the client is not found, we should send broadcast
				    if (dest_mac == NULL) {
					    eth_pdu eth_arp_response;
					    send_broadcast_for_mip(&eth_arp_response, ping_pdu.header.dest_address, epollfd, our_addr);

					    insert_cache_index(&cache, eth_arp_response.mip_pdu.header.source_address, eth_arp_response.header.source_address);
				    	// get address we just inserted
					    dest_mac = get_mac_address(&cache, ping_pdu.header.dest_address);

				    	// b. If the ARP request failed, we cannot reach the given host
					    if (dest_mac == NULL) {
						    printf("ERROR: MIP ARP did not work, aborting ping\n");
					    	continue;
					    }
				    }
					// 6. If the client is found, we should send the ping
				    printf("found MAC address for MIP address\n");
				    size_t mip_pdu_size = ping_pdu.header.sdu_len + sizeof(mip_header);

					eth_header ping_eth_header;
					build_eth_header(&ping_eth_header, dest_mac, our_addr.sll_addr, ETH_MIP_TYPE);

					eth_pdu ping_eth_pdu;
					build_eth_pdu(&ping_eth_pdu, &ping_eth_header, &ping_pdu);

					serialize_eth_pdu(mipd_buffer, &ping_eth_pdu);

				    ssize_t sent_bytes = sendto(raw_sd, mipd_buffer, ETH_HEADER_LEN + mip_pdu_size, 0, (const struct sockaddr *)&our_addr, sizeof(our_addr));
				    if (sent_bytes == -1) {
					    perror("sendto");
					    exit(EXIT_FAILURE);
				    };
				    printf("Sent ping packet:\n");
				}
			}
		}
	}

	close(unix_sd);

	/* Unlink the socket. */
	unlink(socket_upper);

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int opt; 

	printf("%d\n", argc);
	if (argc < 3)
	{
		printf("Too few arguments, socket_upper and MIP address are mandatory: mipd [-h] [-d] <socket_upper> <MIP address>\n");
		exit(EXIT_FAILURE);
	}
	
	while ((opt = getopt(argc, argv, "dh")) != -1) {
		switch (opt) {
		case 'd':
			/* Running in debug mode */
			debug = true;
			break;
		case 'h':
			printf("Usage: %s "
			       "mipd [-h] [-d] <socket_upper> <MIP address>\n", argv[0]);
			exit(0);
		}
	}

	uint8_t pos_arg_start = 1;
	if (debug)
	{
		++pos_arg_start;
		printf("Running in debug mode\n");
	}
	char* socket_upper = argv[pos_arg_start];
	strcpy(socket_name, socket_upper);
	mip_address = atoi(argv[pos_arg_start + 1]);

	printf("%p\n", argv);

	server(socket_upper);

	return 0;
}
