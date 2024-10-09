//
// Created by ikhovind on 02.10.24.
//
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "eth_builder.h"

void build_eth_header(eth_header* header, uint8_t* dest_address, uint8_t* source_address, uint16_t ethertype) {
    memcpy(header->dest_address, dest_address, ETH_ADDR_LEN);
    memcpy(header->source_address, source_address, ETH_ADDR_LEN);
    // network byte order
    header->ethertype = htons(ethertype);
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
    // print buffer
    deserialize_mip_pdu(&target->mip_pdu, buffer + ETH_HEADER_LEN);
}


// Function to print MAC address
void print_mac_address(const char *label, uint8_t address[6], int indent) {
    printf("%*s%s: %02x:%02x:%02x:%02x:%02x:%02x\n", indent, "", label, address[0], address[1], address[2], address[3], address[4], address[5]);
}

// Function to print Ethernet header
void print_eth_header(eth_header header, int indent) {
    print_mac_address("Dest Address", header.dest_address, indent);
    print_mac_address("Source Address", header.source_address, indent);
    printf("%*sEthertype: 0x%04x\n", indent, "", header.ethertype);
}

// Function to print eth_pdu
void print_eth_pdu(eth_pdu* pdu, int indent) {
  	printf("%*sEthernet PDU:\n", indent, "");
    print_eth_header(pdu->header, indent + 4);
    printf("%*sMIP PDU:\n", indent + 4, "");
    print_mip_pdu(&pdu->mip_pdu, indent + 8);
}