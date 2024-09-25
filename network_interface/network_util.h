//
// Created by ikhovind on 22.09.24.
//

#ifndef NETWORK_UTIL_H
#define NETWORK_UTIL_H

#include <stdint.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include "../packet_builder/mip_builder.h"

#define BUF_SIZE 1450

struct ether_frame {
    uint8_t dst_addr[6];
    uint8_t src_addr[6];
    uint8_t eth_proto[2];
    uint8_t contents[0];
} __attribute__((packed));

void get_mac_from_interface(struct sockaddr_ll *so_name);
void get_source_mac_address(int sockfd, const char *interface, unsigned char *mac);
int send_raw_packet(int sd, struct sockaddr_ll *so_name, uint8_t *buf, size_t len);
int recv_raw_packet(int sd, uint8_t *buf, size_t len);
int broadcast(mip_pdu* broadcast_pdu);



#endif //NETWORK_UTIL_H
