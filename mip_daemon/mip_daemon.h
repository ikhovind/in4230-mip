//
// Created by ikhovind on 24.09.24.
//

#ifndef MIP_DAEMON_H
#define MIP_DAEMON_H
#include <stdint.h>
#include "../network_interface/network_util.h"

typedef struct mip_header {
    uint8_t dest_addr: 1;
    uint8_t src_address:   8;
    uint8_t ttl:       4;
    uint16_t sdu_len:   9;
    uint8_t sdu_type:  3;
} mip_header_t;

typedef struct mip_arp_sdu {
    uint8_t type: 1;
    uint8_t address: 8;
} mip_arp_sdu_t;

/* MACROs */
#define MAX_CONNS 5   /* max. length of the pending connections queue */
#define MAX_EVENTS 10 /* max. number of concurrent events to check */

/*
 * We declare the signature of a function in the header file
 * and its definition in the source file.
 *
 * return_type function_name(parameter1, parameter2, ...);
 */

void server(char* socket_name);
int raw_socket();

#endif //MIP_DAEMON_H
