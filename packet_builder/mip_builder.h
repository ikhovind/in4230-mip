//
// Created by ikhovind on 24.09.24.
//
#ifndef MIP_BUILDER_H
#define MIP_BUILDER_H

#include <stdint.h>
#include <stddef.h>

#define PING_SDU_TYPE 0x02
#define ARP_SDU_TYPE 0x01
#define ARP_REQUEST_TYPE 0x0
#define ARP_RESPONSE_TYPE 0x1

typedef struct {
    uint8_t mip_address;
    char* message;
} mip_ping_sdu;

typedef struct {
    uint8_t type: 1;
    uint8_t mip_address;
    uint32_t padding: 23;
} mip_arp_sdu;

typedef struct {
    uint8_t dest_address;
    uint8_t source_address;
    uint8_t ttl: 4;
    uint16_t sdu_len: 16;
    uint8_t sdu_type: 8;
} mip_header;

typedef struct {
    mip_header header;
    void* sdu;
} mip_pdu;

uint8_t* serialize_mip_ping_sdu(mip_ping_sdu* sdu, size_t* size);
mip_ping_sdu* deserialize_mip_ping_sdu(uint8_t* buffer);
uint8_t* serialize_mip_pdu(mip_pdu* sdu, size_t* size);
void deserialize_mip_pdu(mip_pdu* pdu, uint8_t* serial_pdu);
void print_mip_ping_sdu(mip_ping_sdu* sdu);
void build_mip_pdu(mip_pdu* pdu, void* sdu, uint8_t source_address, uint8_t ttl, uint8_t sdu_type);
void print_mip_pdu(mip_pdu* pdu);
void print_mip_arp_sdu(mip_arp_sdu* sdu);


#endif //MIP_BUILDER_H
