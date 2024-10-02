//
// Created by ikhovind on 22.09.24.
//

#ifndef NETWORK_UTIL_H
#define NETWORK_UTIL_H

#include <linux/if_packet.h>
#include "../packet_builder/mip_builder.h"

#define BUF_SIZE 1450

void get_mac_from_interface(struct sockaddr_ll *so_name);


#endif //NETWORK_UTIL_H
