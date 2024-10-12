//
// Created by ikhovind on 02.10.24.
//
#include "mip_builder.h"
#include <stdint.h>

#ifndef ETH_BUILDER_H
#define ETH_BUILDER_H

#define ETH_ADDR_LEN 6
#define ETH_TYPE_LEN 2
#define ETH_HEADER_LEN (ETH_ADDR_LEN * 2 + ETH_TYPE_LEN)

#define ETH_P_MIP 0x88b5

typedef struct {
    uint8_t dest_address[6];
    uint8_t source_address[6];
    uint16_t ethertype;
} eth_header;

typedef struct {
    eth_header header;
    mip_pdu mip_pdu;
} eth_pdu;

enum {
    ETH_HEADER_SIZE = sizeof(eth_header),
    ETH_PDU_MAX_SIZE = ETH_HEADER_SIZE + MIP_PDU_MAX_SIZE,

    ETH_ARP_SIZE = ETH_HEADER_SIZE + MIP_ARP_PDU_SIZE,
};

void build_eth_header(eth_header* header, uint8_t* dest_address, uint8_t* source_address, uint16_t ethertype);
void build_eth_pdu(eth_pdu* pdu, eth_header* header, mip_pdu* mip_pdu);

void serialize_eth_pdu(uint8_t* target, eth_pdu* pdu);
void deserialize_eth_pdu(eth_pdu* target, uint8_t* buffer);
void print_mac_address(const char *label, uint8_t address[6], int indent);
void print_eth_header(eth_header header, int indent);
void print_eth_pdu(eth_pdu* pdu, int indent);



#endif //ETH_BUILDER_H