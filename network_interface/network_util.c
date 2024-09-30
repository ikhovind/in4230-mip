//
// Created by ikhovind on 22.09.24.
//

#define _DEFAULT_SOURCE
#include "network_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <linux/if_packet.h>  // for struct sockaddr_ll
#include <linux/if_ether.h>   // for ETH_FRAME_LEN


#include <errno.h>
#include <arpa/inet.h>

static uint8_t dst_addr[6];

void get_mac_from_interface(struct sockaddr_ll *so_name) {
    struct ifaddrs *ifaces, *ifp;
    if (getifaddrs(&ifaces)) {
        perror("getifaddrs");
        exit(-1);
    }

    for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
        if (ifp->ifa_addr != NULL &&
            ifp->ifa_addr->sa_family == AF_PACKET &&
            (strcmp("lo", ifp->ifa_name)))
            memcpy(so_name, (struct sockaddr_ll*)ifp->ifa_addr, sizeof(struct sockaddr_ll));
    }
    printf("printing sll_addr\n");
    for (int i = 0; i < 8; ++i) {
        printf("%d", so_name->sll_addr[i]);
    }
    printf("\n");
    printf("%d\n", so_name->sll_addr);

    freeifaddrs(ifaces);
}

void get_source_mac_address(int sockfd, const char *interface, unsigned char *mac) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("Failed to get MAC address");
        exit(EXIT_FAILURE);
    }

    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
}

int send_raw_packet(int sd, struct sockaddr_ll *so_name, uint8_t *buf, size_t len) {
    struct ether_frame frame_hdr;
    struct msghdr *msg;
    struct iovec msgvec[2];
    int rc;

    memcpy(frame_hdr.dst_addr, dst_addr, 6);
    memcpy(frame_hdr.src_addr, so_name->sll_addr, 6);
    frame_hdr.eth_proto[0] = frame_hdr.eth_proto[1] = 0xFF;

    msgvec[0].iov_base = &frame_hdr;
    msgvec[0].iov_len = sizeof(struct ether_frame);
    msgvec[1].iov_base = buf;
    msgvec[1].iov_len = len;

    msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));
    msg->msg_name = so_name;
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iovlen = 2;
    msg->msg_iov = msgvec;

    rc = sendmsg(sd, msg, 0);
    if (rc == -1) {
        perror("sendmsg");
        free(msg);
        return 1;
    }

    free(msg);
    return 0;
}

int recv_raw_packet(int sd, uint8_t *buf, size_t len) {
    struct sockaddr_ll so_name;
    struct ether_frame frame_hdr;
    struct msghdr msg;
    struct iovec msgvec[2];
    int rc;

    msgvec[0].iov_base = &frame_hdr;
    msgvec[0].iov_len = sizeof(struct ether_frame);
    msgvec[1].iov_base = buf;
    msgvec[1].iov_len = len;

    msg.msg_name = &so_name;
    msg.msg_namelen = sizeof(struct sockaddr_ll);
    msg.msg_iovlen = 2;
    msg.msg_iov = msgvec;

    rc = recvmsg(sd, &msg, 0);
    if (rc == -1) {
        perror("recvmsg");
        return -1;
    }

    memcpy(dst_addr, frame_hdr.src_addr, 6);
    return rc;
}


void get_eth_interface(char* eth_interface) {
    struct ifaddrs *interfaces, *temp;
    int sockfd;

    // Get interface list
    if (getifaddrs(&interfaces) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Loop through the list of interfaces
    for (temp = interfaces; temp != NULL; temp = temp->ifa_next) {
        if (temp->ifa_addr == NULL) continue;
        // Check for AF_INET family (IPv4)
        if (temp->ifa_addr->sa_family == AF_INET) {
            char addr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)temp->ifa_addr)->sin_addr, addr, INET_ADDRSTRLEN);
            strcpy(eth_interface, temp->ifa_name);
        }
    }
}

#define INTERFACE "h1-eth0"
#define BROADCAST_MAC "\xff\xff\xff\xff\xff\xff"

int broadcast(mip_pdu* broadcast_pdu) {
    printf("broadcasting\n");
    // Creates a raw socket
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }


    struct ifaddrs *ifaces, *ifp;
    if (getifaddrs(&ifaces)) {
        perror("getifaddrs");
        exit(-1);
    }

    char buffer[32];
    for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
        if (ifp->ifa_addr != NULL && ifp->ifa_addr->sa_family == AF_PACKET && (strcmp("lo", ifp->ifa_name))) {
            memcpy(buffer, (struct sockaddr_ll*)ifp->ifa_name, sizeof(struct sockaddr_ll));
            printf("buffer is: %s\n", ifp->ifa_name);
        }
    }
    printf("buffer is: %s\n", buffer);
    printf("buflen is: %d\n", strlen(buffer));

    // Get the index of the network interface
    struct ifreq ifr;

    char eth_interface[IFNAMSIZ];
    get_eth_interface(eth_interface);
    strncpy(ifr.ifr_name, eth_interface, strlen(eth_interface));
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("Failed to get interface index\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    int ifindex = ifr.ifr_ifindex;

    // Get the MAC address of the interface
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        perror("Failed to get MAC address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    unsigned char source_mac[6];
    memcpy(source_mac, ifr.ifr_hwaddr.sa_data, 6);

    printf("broadcasting pdu:\n");
    print_mip_pdu(broadcast_pdu);
    uint8_t* serial_pdu = malloc(sizeof(mip_header) + broadcast_pdu->header.sdu_len);
    serialize_mip_pdu(serial_pdu, broadcast_pdu);

    // src[6] + dst[6] + type [2]
    uint8_t length = 14 * sizeof(uint8_t) + sizeof(mip_header) + broadcast_pdu->header.sdu_len;
    // Construct the Ethernet frame
    uint8_t frame[length];
    memset(frame, 0, length);

    // Destination MAC address (broadcast)
    memcpy(frame, BROADCAST_MAC, 6);
    // Source MAC address
    memcpy(frame + 6, source_mac, 6);
    // EtherType (custom value for example)
    frame[12] = 0x88;
    frame[13] = 0xb5;

    memcpy(frame + 14, serial_pdu, sizeof(mip_header) + broadcast_pdu->header.sdu_len);

    // Set the destination address
    struct sockaddr_ll addr = {0};
    addr.sll_ifindex = ifindex;
    addr.sll_halen = ETH_ALEN;
    memcpy(addr.sll_addr, BROADCAST_MAC, ETH_ALEN);

    // Send the Ethernet frame
    if (sendto(sockfd, frame, sizeof(frame), 0, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Failed to send the frame");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Broadcast packet sent successfully!\n");

    close(sockfd);
    return 0;
}