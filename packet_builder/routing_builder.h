#ifndef ROUTING_BUILDER_H
#define ROUTING_BUILDER_H

#include <stdint.h>
typedef struct requst_pkg {
    uint8_t source_mip; // MIP address of the host itself
    uint8_t ttl;
    uint8_t R; // R ascii code, 0x52
    uint8_t E; // E ascii code 0x45
    uint8_t Q; // Q ascii code 0x51
    uint8_t lookup_mip; // MIP address to lookup
} Request;

typedef struct response_pkg {
    uint8_t source_mip; // MIP address of the host itself
    uint8_t ttl;
    uint8_t R;
    uint8_t S;
    uint8_t P;
    uint8_t next_hop_mip; // next hop MIP address
} Response;

#endif //ROUTING_BUILDER_H
