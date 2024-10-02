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
#include <unistd.h>
#include <linux/if_packet.h>  // for struct sockaddr_ll


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
    freeifaddrs(ifaces);
}