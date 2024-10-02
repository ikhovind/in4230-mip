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
#define ETH_PDU_MAX_SIZE (MIP_PDU_MAX_SIZE + ETH_HEADER_LEN)
#define ETH_ARP_SIZE (ETH_HEADER_LEN + MIP_ARP_SDU_SIZE)

#define ETH_MIP_TYPE 0xb588

typedef struct {
    uint8_t dest_address[6];
    uint8_t source_address[6];
    uint16_t ethertype;
} eth_header;

typedef struct {
    eth_header header;
    mip_pdu mip_pdu;
} eth_pdu;

void build_eth_header(eth_header* header, uint8_t* dest_address, uint8_t* source_address, uint16_t ethertype);
void build_eth_pdu(eth_pdu* pdu, eth_header* header, mip_pdu* mip_pdu);

void serialize_eth_pdu(uint8_t* target, eth_pdu* pdu);
void deserialize_eth_pdu(eth_pdu* target, uint8_t* buffer);



#endif //ETH_BUILDER_H
