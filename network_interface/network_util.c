//
// Created by ikhovind on 22.09.24.
//

#include "network_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/ethernet.h>

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