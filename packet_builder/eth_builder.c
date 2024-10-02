//
// Created by ikhovind on 02.10.24.
//
#include <stdint.h>
#include "eth_builder.h"
#include <string.h>

void build_eth_header(eth_header* header, uint8_t* dest_address, uint8_t* source_address, uint16_t ethertype) {
    memcpy(header->dest_address, dest_address, ETH_ADDR_LEN);
    memcpy(header->source_address, source_address, ETH_ADDR_LEN);
    header->ethertype = ethertype;
}

void build_eth_pdu(eth_pdu* eth_pdu, eth_header* header, mip_pdu* mip_pdu) {
    eth_pdu->header = *header;
    eth_pdu->mip_pdu = *mip_pdu;
}

void serialize_eth_pdu(uint8_t* target, eth_pdu* eth_pdu) {
    memcpy(target, eth_pdu, ETH_HEADER_LEN);
    serialize_mip_pdu(target + ETH_HEADER_LEN, &eth_pdu->mip_pdu);
}

void deserialize_eth_pdu(eth_pdu* target, uint8_t* buffer) {
    memcpy(target, buffer, ETH_HEADER_LEN);
    deserialize_mip_pdu(&target->mip_pdu, buffer + ETH_HEADER_LEN);
}
