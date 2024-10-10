//
// Created by ikhovind on 22.09.24.
//

#ifndef NETWORK_UTIL_H
#define NETWORK_UTIL_H

#include <linux/if_packet.h>
#include "../packet_builder/mip_builder.h"

#define BUF_SIZE 1450

void get_interface_mac_address(unsigned char local_mac[6], int sll_ifindex, int raw_sd);


#endif //NETWORK_UTIL_H
