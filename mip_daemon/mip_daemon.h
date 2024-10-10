//
// Created by ikhovind on 24.09.24.
//

#ifndef MIP_DAEMON_H
#define MIP_DAEMON_H
#include <stdint.h>
#include "../packet_builder/mip_builder.h"

typedef struct arp_context {
    mip_pdu awaiting_arp_response[1];
    bool is_waiting_packet;
} arp_context;

/* MACROs */
#define MAX_CONNS 5   /* max. length of the pending connections queue */
#define MAX_EVENTS 10 /* max. number of concurrent events to check */
#define BROADCAST_MAC "\xff\xff\xff\xff\xff\xff"


void server(char* socket_name);
int raw_socket();

#endif //MIP_DAEMON_H
