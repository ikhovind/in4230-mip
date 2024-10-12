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
#include <signal.h>
#include <bits/sigaction.h>

#include <netinet/in.h>

#include <fcntl.h>
#include <ifaddrs.h>

#include "../packet_builder/eth_builder.h"
#include "../cache/arp_cache.h"
#include "../network_interface/network_util.h"
#include "../packet_builder/mip_builder.h"


/**
 * @brief Struct to hold a packet that is waiting for an ARP response
 *
 * If the destination MIP address is not in the ARP cache, the packet will be stored here
 * is_waiting_packet flag indicates if there is a packet waiting for an ARP response
 *
 * Waiting packet will be overwritten if a new packet is sent while waiting
 */
typedef struct {
    mip_pdu awaiting_arp_response[1];
    bool is_waiting_packet;
} PacketCache;

/* MACROs */
#define MAX_CONNS 5   /* max. length of the pending connections queue */
#define MAX_EVENTS 10 /* max. number of concurrent events to check */
#define BROADCAST_MAC "\xff\xff\xff\xff\xff\xff"

// stores the MIP address of this daemon, specified by the user
static uint8_t mip_address;
// Caches values after ARP requests
static ArpCache arp_cache;
// Caches packets that are waiting for an ARP response
static PacketCache packet_cache;
// for communicating with nodes over unix socket
uint8_t node_buffer[MIP_SDU_MAX_LENGTH];
// for communicating with other daemons over raw socket
uint8_t mipd_buffer[ETH_PDU_MAX_SIZE];

int unix_sd;
int raw_sd;
bool debug = false;

/**
 * @brief Function to hand to sa_action to clean up sockets on exit
 *
 * @param signum Signal number
 */
static void cleanup(int signum);

/**
 * @brief Function to prepare and listen on unix socket
 *
 * @param socket_upper Null-terminated name of the socket, \0 will be added to the start of the name when creating the socket
 *					   Max length is therefore 107 characters
 * @return int The file descriptor of the socket
 */
static int prepare_unix_socket(const char* socket_upper);
/**
 * @brief Function to add a file descriptor to the epoll table
 *
 * @param efd The epoll file descriptor
 * @param ev The epoll event struct
 * @param fd The file descriptor to add
 * @return int 0 on success, -1 on failure
 */
static int add_to_epoll_table(int efd, struct epoll_event *ev, int fd);
/**
 * @brief Function to send a packet on the raw socket, using the raw_sd global variable
 *
 * @param packet The packet to send
 * @param mac_dest The MAC address of the destination
 * @param addr The sockaddr_ll struct of the interface to send the packet on
 */
static void send_packet_on_raw_socket(const mip_pdu* packet, const uint8_t* mac_dest, const struct sockaddr_ll* addr);

/**
 * @brief Broadcast MIP-ARP request on all available interfaces, looking for a given MIP address
 *
 * @param mip_dest_address The MIP address to send the broadcast for
 */
static void send_broadcast_for_mip(uint8_t mip_dest_address);

/**
 * @brief Try to send any packet currently in the packet_cache
 *
 * Should be invoked every time the ARP cache is updated
 *
 * Checks if the destination MIP address is in the arp_cache, if so, sends the packet and resets the waiting flag
 *
 * If the destination is not in the arp_cache, does nothing
 * If the is_waiting_packet flag is not set, does nothing
 */
static void send_waiting_ping_if_possible();
/**
 * @brief Main event-loop server function
 *
 * @param socket_upper The name of the socket to communicate with any node, max length is 107 characters
 *                     When creating the socket, a \0 will be added to the start of the name to make it an abstract namespace socket
 */
static void server(const char* socket_upper);

/**
 * @brief Entry point for the program parses arguments and starts the server
 *
 * @param argc Number of arguments
 * @param argv Array of arguments
 * @return int 0 on success, -1 on failure
 */
int main(int argc, char **argv);


static void cleanup(const int signum) {
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

static int prepare_unix_socket(const char* socket_upper)
{
	int sd = -1, rc = -1;
	struct sockaddr_un addr;

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

	char sock_path_buf[sizeof(addr.sun_path)];

	// abstract namespace so we do not have to worry about closing the socket
	sock_path_buf[0] = '\0';
	// reserve space for the abstract namespace character
	strncpy(sock_path_buf + 1, socket_upper, sizeof(addr.sun_path) - 1);

	/* Why `-1`??? Check character arrays in C!
	 * '\0', 's' 'e' 'r' 'v' 'e' 'r' '.' 's' 'o' 'c' 'k' 'e' 't' '\0'
	 * sizeof() counts the final null-terminated character ('\0') as well.
	 */
	strncpy(addr.sun_path, sock_path_buf, sizeof(addr.sun_path) - 1);

	/* No need to unlink as we are using abstract namespace sockets
	 * Check https://gavv.github.io/articles/unix-socket-reuse
	 */
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

static void send_packet_on_raw_socket(const mip_pdu* packet, const uint8_t* mac_dest, const struct sockaddr_ll* addr) {
	const size_t mip_pdu_size = packet->header.sdu_len * 4 + sizeof(mip_header);

	// source MAC address
	unsigned char local_mac[6];
	get_interface_mac_address(local_mac, addr->sll_ifindex, raw_sd);

	eth_header ping_eth_header;
	build_eth_header(&ping_eth_header, mac_dest, local_mac, ETH_P_MIP);

	eth_pdu ping_eth_pdu;
	build_eth_pdu(&ping_eth_pdu, &ping_eth_header, packet);

	serialize_eth_pdu(mipd_buffer, &ping_eth_pdu);

	if (debug) {
		printf("Sending eth packet:\n");
		print_eth_pdu(&ping_eth_pdu, 4);
		print_arp_cache(&arp_cache, 4);
	}

	ssize_t sent_bytes = sendto(raw_sd, mipd_buffer, ETH_HEADER_LEN + mip_pdu_size, 0, (const struct sockaddr *)addr, sizeof(*addr));
	if (sent_bytes == -1) {
		perror("sendto");
		exit(EXIT_FAILURE);
	};
}

static int add_to_epoll_table(const int efd, struct epoll_event *ev, const int fd)
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

static void send_broadcast_for_mip(uint8_t mip_dest_address) {
	// first send broadcast
	mip_arp_sdu arp_sdu;
	arp_sdu.type = ARP_REQUEST_TYPE;
	// mip address to look up
	arp_sdu.mip_address = mip_dest_address;
	mip_pdu mip_pdu;

	build_mip_pdu(&mip_pdu, &arp_sdu, mip_address, mip_dest_address, 1, ARP_SDU_TYPE);
	struct ifaddrs *ifaces, *ifp;
    if (getifaddrs(&ifaces)) {
		perror("getifaddrs");
		exit(-1);
    }

	// send broadcast on all interfaces
    for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
        if (ifp->ifa_addr != NULL && ifp->ifa_addr->sa_family == AF_PACKET) {
        	// Avoid loopback so we don't send to ourselves
        	if (strcmp("lo", ifp->ifa_name) != 0) {
        		send_packet_on_raw_socket(&mip_pdu, BROADCAST_MAC, (struct sockaddr_ll*)ifp->ifa_addr);
        	}
        }
    }
	freeifaddrs(ifaces);
}

static void send_waiting_ping_if_possible() {
	if (!packet_cache.is_waiting_packet) {
		return;
	}
	// get the first packet in the cache
	const mip_pdu* packet = &packet_cache.awaiting_arp_response[0];
	// get the mac address of the destination
	const ArpCacheIndex* dest = get_mac_address(&arp_cache, packet->header.dest_address);
	// if the destination is in the cache, send the packet
	if (dest != NULL) {
		// 6. If the client is found, we should send the ping
		if (debug) {
			printf("Sending ping packet which was cached:\n");
		}
		send_packet_on_raw_socket(packet, dest->mac_address, &dest->ll_addr);
		free(((mip_ping_sdu*)packet->sdu)->message);
		packet_cache.is_waiting_packet = 0;
	}
}


static void server(const char* socket_upper)
{
	struct epoll_event ev, events[MAX_EVENTS];
	int	   accept_sd = -1;
	int epollfd, rc;


	// Set up signal handler for SIGINT
	struct sigaction sa;
	sa.sa_handler = cleanup;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

	printf("\n*** IN4230 MIPD server! ***\n\n");

	/* Call the method to create, bind and listen to the server socket */
	unix_sd = prepare_unix_socket(socket_upper);

	raw_sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));
	if (raw_sd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

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


				if (debug) {
					printf("New node connected...\n");
				}

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
				 */
				int fd = events[i].data.fd;
				const int unix_sd = accept_sd;
				struct sockaddr_ll recv_addr;
				socklen_t addr_len = sizeof(recv_addr);
				//* We should:
				//*  1. Read the data
				const ssize_t received_bytes = recvfrom(
					fd,
					mipd_buffer,
					sizeof(mipd_buffer),
					0,
					(struct sockaddr *) &recv_addr,
					&addr_len);
				if (received_bytes <= 0) {
					close(fd);
					exit(EXIT_FAILURE);
				}

				eth_pdu received_eth_pdu;
				//*  2. Deserialize the packet
				deserialize_eth_pdu(&received_eth_pdu, mipd_buffer);
				if (debug) {
					printf("Received eth pdu on raw socket:\n");
					print_eth_pdu(&received_eth_pdu, 4);
					print_arp_cache(&arp_cache, 4);
				}

				//*  3. Check if it is a broadcast or a ping
				if (received_eth_pdu.mip_pdu.header.sdu_type == ARP_SDU_TYPE) {
					if (((mip_arp_sdu*)received_eth_pdu.mip_pdu.sdu)->type == ARP_RESPONSE_TYPE) {
						insert_cache_index(&arp_cache, received_eth_pdu.mip_pdu.header.source_address, received_eth_pdu.header.source_address, recv_addr);
						send_waiting_ping_if_possible();
					}
					else if (((mip_arp_sdu*)received_eth_pdu.mip_pdu.sdu)->type == ARP_REQUEST_TYPE) {
						// If broadcast then we first learn the MAC address of the sender
						insert_cache_index(&arp_cache, received_eth_pdu.mip_pdu.header.source_address, received_eth_pdu.header.source_address, recv_addr);
						const uint8_t desired_mip_adr = ((mip_arp_sdu*)received_eth_pdu.mip_pdu.sdu)->mip_address;

						// Then we check if the sender is looking for us
						if (desired_mip_adr == mip_address) {
							// If the sender was looking for us, then respond

							mip_arp_sdu arp_sdu_response;
							arp_sdu_response.type = ARP_RESPONSE_TYPE;
							arp_sdu_response.mip_address = mip_address;

							mip_pdu mip_pdu;
							build_mip_pdu(&mip_pdu, &arp_sdu_response, mip_address, received_eth_pdu.mip_pdu.header.source_address, 1, ARP_SDU_TYPE);

							ArpCacheIndex* dest = get_mac_address(&arp_cache, received_eth_pdu.mip_pdu.header.source_address);
							send_packet_on_raw_socket(&mip_pdu, dest->mac_address, &dest->ll_addr);
						}
						else {
							printf("Received broadcast not for us, ignoring\n");
						}
					}
				}
				//*     b. If it is a ping, we should forward it to the correct client
				if (received_eth_pdu.mip_pdu.header.sdu_type == PING_SDU_TYPE) {

					mip_ping_sdu ping_sdu = {
						// swap source and dest addresses so that if there is a response, it will be sent to the correct node
						.mip_address = received_eth_pdu.mip_pdu.header.source_address,
						.message = ((mip_ping_sdu*) received_eth_pdu.mip_pdu.sdu)->message,
					};

					serialize_mip_ping_sdu(node_buffer, &ping_sdu);
					// check if unix_sd is a valid file descriptor
					if(fcntl(unix_sd, F_GETFD) != -1) {
						if (debug) {
							printf("Unix file descriptor is valid, forwarding received ping SDU to node:\n");
							print_mip_ping_sdu(&ping_sdu, 4);
							print_arp_cache(&arp_cache, 4);
						}
						size_t written_bytes = write(unix_sd, node_buffer, sizeof(uint8_t) + strlen(ping_sdu.message) + 1);
						if (written_bytes == -1) {
							perror("write");
							exit(EXIT_FAILURE);
						}
					}
					else if (debug) {
						printf("Unix file descriptor is invalid, cannot forward received ping SDU\n");
					}
				}
				free_mip_pdu(&received_eth_pdu.mip_pdu);
			} else {
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
					if (debug) {
						printf("Node closed communication\n");
					}
					close(node_fd);
				}
				else {
					// if we have received a message from the node
				    // 2. Deserialize the packet
					mip_ping_sdu ping_sdu;
					deserialize_mip_ping_sdu(&ping_sdu, node_buffer);
					if (debug) {
						printf("Received ping SDU from node:\n");
						print_mip_ping_sdu(&ping_sdu, 4);
						print_arp_cache(&arp_cache, 4);
					}

					// 3. Build the MIP PDU
				    mip_pdu ping_pdu;
				    build_mip_pdu(&ping_pdu, &ping_sdu, mip_address, ping_sdu.mip_address, 1, PING_SDU_TYPE);

					// 4. Check the cache for the destination MIP address
					ArpCacheIndex *dest = get_mac_address(&arp_cache, ping_pdu.header.dest_address);
					// 5. If the client is not found, we should send broadcast
				    if (dest == NULL) {
				    	// Send broadcast and cache the packet, send it later when we receive a ARP response
					    send_broadcast_for_mip(ping_pdu.header.dest_address);
				    	// this will be sent when we later receive a response
				    	// must wait to free memory until we have sent the packet
				    	packet_cache.awaiting_arp_response[0] = ping_pdu;
				    	packet_cache.is_waiting_packet = 1;
				    } else {
						// If we have it in our cache
						if (debug) {
							printf("Sending ping packet:\n");
						}
				    	send_packet_on_raw_socket(&ping_pdu, dest->mac_address, &dest->ll_addr);
				    	// we have sent the packet and can free the allocated memory
				    	free(((mip_ping_sdu*)ping_pdu.sdu)->message);
					}
				}
			}
		}
	}
}

int main(const int argc, char **argv)
{
	int opt; 

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
	const char* socket_upper = argv[pos_arg_start];
	mip_address = strtol(argv[pos_arg_start + 1], NULL, 10);

	printf("%p\n", argv);

	server(socket_upper);

	return 0;
}
