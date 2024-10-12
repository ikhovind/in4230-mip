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

#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if.h>

#include "../packet_builder/eth_builder.h"
#include "../cache/arp_cache.h"
#include "../network_interface/network_util.h"

static uint8_t mip_address;
static arp_cache cache;
static arp_context awaiting_arp;
// for communicating with nodes over unix socket
uint8_t node_buffer[MIP_SDU_MAX_LENGTH];
// for communicating with other mips over raw socket
uint8_t mipd_buffer[ETH_PDU_MAX_SIZE];

int unix_sd;
int raw_sd;
bool debug = false;

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

int raw_socket()
{
	int	raw_sock;

	raw_sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_MIP));
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

	char sock_path_buf[sizeof(addr.sun_path)];

	sock_path_buf[0] = '\0';
	// reserve space for the abstract namespace character
	strncpy(sock_path_buf + 1, socket_upper, sizeof(addr.sun_path) - 1);

	/* Why `-1`??? Check character arrays in C!
	 * '\0', 's' 'e' 'r' 'v' 'e' 'r' '.' 's' 'o' 'c' 'k' 'e' 't' '\0'
	 * sizeof() counts the final null-terminated character ('\0') as well.
	 *
	 *
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


int broadcast_on_interface(mip_pdu* broadcast_pdu, struct sockaddr_ll addr) {
    // Construct the Ethernet frame
	eth_header broadcast_header;

	unsigned char local_mac[6];
	get_interface_mac_address(local_mac, addr.sll_ifindex, raw_sd);

	build_eth_header(&broadcast_header, BROADCAST_MAC, local_mac, ETH_P_MIP);
	eth_pdu broadcast_eth_pdu;
	build_eth_pdu(&broadcast_eth_pdu, &broadcast_header, broadcast_pdu);

	serialize_eth_pdu(mipd_buffer, &broadcast_eth_pdu);

	if (debug) {
		printf("Sending ARP request:\n");
		print_eth_pdu(&broadcast_eth_pdu, 4);
		print_sockaddr_ll(addr, 4);
		print_arp_cache(&cache, 0);
	}

    // Send the Ethernet frame
    if (sendto(raw_sd, mipd_buffer, ETH_ARP_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Failed to send the frame");
        close(raw_sd);
        exit(EXIT_FAILURE);
    }
    return 0;
}

void send_broadcast_for_mip(uint8_t mip_dest_address) {
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

    for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
        if (ifp->ifa_addr != NULL && ifp->ifa_addr->sa_family == AF_PACKET) {
        	// Avoid loopback so we don't send to ourselves
        	if (strcmp("lo", ifp->ifa_name) != 0) {
        		broadcast_on_interface(&mip_pdu, *(struct sockaddr_ll*)ifp->ifa_addr);
        	}
        }
    }
	freeifaddrs(ifaces);
}

// Check if the packet in arp_context can be sent with the data currently in our cache
void send_waiting_ping_if_possible() {
	if (!awaiting_arp.is_waiting_packet) {
		return;
	}
	// get the first packet in the context
	mip_pdu* packet = &awaiting_arp.awaiting_arp_response[0];
	// get the mac address of the destination
	printf("addr of packet is: %p\n", packet);
	arp_cache_index* dest = get_mac_address(&cache, packet->header.dest_address);
	// if the destination is in the cache, send the packet
	if (dest != NULL) {
		// 6. If the client is found, we should send the ping
		size_t mip_pdu_size = packet->header.sdu_len + sizeof(mip_header);

		unsigned char local_mac[6];
		get_interface_mac_address(local_mac, dest->ll_addr.sll_ifindex, raw_sd);

		eth_header ping_eth_header;
		build_eth_header(&ping_eth_header, dest->mac_address, local_mac, ETH_P_MIP);

		eth_pdu ping_eth_pdu;
		build_eth_pdu(&ping_eth_pdu, &ping_eth_header, packet);

		serialize_eth_pdu(mipd_buffer, &ping_eth_pdu);

		if (debug) {
			printf("Sending ping packet from context:\n");
			print_eth_pdu(&ping_eth_pdu, 4);
			print_arp_cache(&cache, 0);
		}
		printf("Setting awaiting packet to zero\n");
		awaiting_arp.is_waiting_packet = 0;

		ssize_t sent_bytes = sendto(raw_sd, mipd_buffer, ETH_HEADER_LEN + mip_pdu_size, 0, (const struct sockaddr *)&dest->ll_addr, sizeof(dest->ll_addr));
		if (sent_bytes == -1) {
			perror("sendto");
			exit(EXIT_FAILURE);
		};

	}
}


void server(char* socket_upper)
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
				int unix_sd = accept_sd;
				int rc;
				struct sockaddr_ll recv_addr;
				socklen_t addr_len = sizeof(recv_addr);
				//* We should:
				//*  1. Read the data
				rc = recvfrom(fd, mipd_buffer, sizeof(mipd_buffer), 0, (struct sockaddr *)&recv_addr, &addr_len);
				if (rc <= 0) {
					close(fd);
					exit(EXIT_FAILURE);
				}

				eth_pdu received_eth_pdu;
				//*  2. Deserialize the packet
				deserialize_eth_pdu(&received_eth_pdu, mipd_buffer);
				if (debug) {
					printf("Received eth pdu on raw socket:\n");
					print_eth_pdu(&received_eth_pdu, 4);
					print_arp_cache(&cache, 4);
				}

				//*  3. Check if it is a broadcast or a ping
				if (received_eth_pdu.mip_pdu.header.sdu_type == ARP_SDU_TYPE) {
					if (((mip_arp_sdu*)received_eth_pdu.mip_pdu.sdu)->type == ARP_RESPONSE_TYPE) {
						insert_cache_index(&cache, received_eth_pdu.mip_pdu.header.source_address, received_eth_pdu.header.source_address, recv_addr);
						send_waiting_ping_if_possible();
					}
					else if (((mip_arp_sdu*)received_eth_pdu.mip_pdu.sdu)->type == ARP_REQUEST_TYPE) {
						// If broadcast then we first learn the MAC address of the sender
						insert_cache_index(&cache, received_eth_pdu.mip_pdu.header.source_address, received_eth_pdu.header.source_address, recv_addr);
						unsigned char desired_mip_adr = ((mip_arp_sdu*)received_eth_pdu.mip_pdu.sdu)->mip_address;

						// Then we check if the sender is looking for us
						if (desired_mip_adr == mip_address) {
							// If the sender was looking for us, then respond

							mip_arp_sdu arp_sdu_response;
							arp_sdu_response.type = ARP_RESPONSE_TYPE;
							arp_sdu_response.mip_address = mip_address;

							mip_pdu mip_pdu;
							build_mip_pdu(&mip_pdu, &arp_sdu_response, mip_address, received_eth_pdu.mip_pdu.header.source_address, 1, ARP_SDU_TYPE);

							unsigned char local_mac[6];
							get_interface_mac_address(local_mac, recv_addr.sll_ifindex, raw_sd);

							eth_header eth_arp_response_header;
							build_eth_header(&eth_arp_response_header, received_eth_pdu.header.source_address, local_mac, ETH_P_MIP);

							eth_pdu eth_arp_response;
							build_eth_pdu(&eth_arp_response, &eth_arp_response_header, &mip_pdu);

							serialize_eth_pdu(mipd_buffer, &eth_arp_response);
							if (debug) {
								printf("Sending ARP response\n");
								print_eth_pdu(&eth_arp_response, 4);
								print_arp_cache(&cache, 0);
							}
							ssize_t sent_bytes = sendto(fd, mipd_buffer, ETH_ARP_SIZE, 0, (const struct sockaddr *)&recv_addr, sizeof(recv_addr));
							if (sent_bytes == -1) {
								perror("sendto");
								exit(EXIT_FAILURE);
							};
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
							print_arp_cache(&cache, 0);
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
				    // 2. Deserialize the packet
					mip_ping_sdu ping_sdu;
					deserialize_mip_ping_sdu(&ping_sdu, node_buffer);
					if (debug) {
						printf("Received ping SDU from node:\n");
						print_mip_ping_sdu(&ping_sdu, 4);
						print_arp_cache(&cache, 0);
					}

					// 3. Build the MIP PDU
				    mip_pdu ping_pdu;
				    build_mip_pdu(&ping_pdu, &ping_sdu, mip_address, ping_sdu.mip_address, 1, PING_SDU_TYPE);

					// 4. Check the cache for the destination MIP address
					arp_cache_index *dest = get_mac_address(&cache, ping_pdu.header.dest_address);
					// 5. If the client is not found, we should send broadcast
				    if (dest == NULL) {
					    send_broadcast_for_mip(ping_pdu.header.dest_address);
				    	// this will be sent when we later receive a response
				    	awaiting_arp.awaiting_arp_response[0] = ping_pdu;
				    	awaiting_arp.is_waiting_packet = 1;
				    } else {
						// If we have it in our cache

						// 6. If the client is found, we should send the ping
						size_t mip_pdu_size = ping_pdu.header.sdu_len + sizeof(mip_header);

				    	unsigned char local_mac[6];

				    	get_interface_mac_address(local_mac, dest->ll_addr.sll_ifindex, raw_sd);


						eth_header ping_eth_header;
						build_eth_header(&ping_eth_header, dest->mac_address, local_mac, ETH_P_MIP);

						eth_pdu ping_eth_pdu;
						build_eth_pdu(&ping_eth_pdu, &ping_eth_header, &ping_pdu);

						serialize_eth_pdu(mipd_buffer, &ping_eth_pdu);

						if (debug) {
							printf("Sending ping packet:\n");
							print_eth_pdu(&ping_eth_pdu, 4);
							print_arp_cache(&cache, 0);
						}

						ssize_t sent_bytes = sendto(raw_sd, mipd_buffer, ETH_HEADER_LEN + mip_pdu_size, 0, (const struct sockaddr *)&dest->ll_addr, sizeof(dest->ll_addr));
						if (sent_bytes == -1) {
							perror("sendto");
							exit(EXIT_FAILURE);
						};

					}
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
	mip_address = atoi(argv[pos_arg_start + 1]);

	printf("%p\n", argv);

	server(socket_upper);

	return 0;
}
