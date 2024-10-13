#include "mip_builder.h"

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

size_t serialize_mip_ping_sdu(uint8_t* target, const mip_ping_sdu* sdu) {
    // Copy mip_address to buffer
    target[0] = sdu->mip_address;

    // Copy message to buffer after mip_address
    strcpy((char*) target + 1, sdu->message);
    // include null-terminator and mip_address
    size_t written_size = strlen(sdu->message) + 1 + 1;
    // set rest of buffer to 0
    memset(target + written_size, 0, 4 - written_size % 4);
    // Pad up to nearest 4 bytes
    return written_size + (4 - written_size % 4) % 4;
}

void deserialize_mip_ping_sdu(mip_ping_sdu* target, const uint8_t* buffer) {
    // Copy mip_address from buffer
    target->mip_address = buffer[0];

    // allocate memory for message and null-terminator
    target->message = malloc(strlen((char*)(buffer + 1)) + 1);
    strcpy(target->message, (char*)(buffer + 1));
}

void serialize_mip_pdu(uint8_t* target, const mip_pdu* pdu) {
    if (pdu->header.sdu_type == PING_SDU_TYPE) {
        mip_ping_sdu* ping_sdu = (mip_ping_sdu*)pdu->sdu;
        // copy header
        memcpy(target, &pdu->header, sizeof(mip_header));

        // copy sdu
        serialize_mip_ping_sdu(target + sizeof(mip_header), ping_sdu);
        return;
    }
    if (pdu->header.sdu_type == ARP_SDU_TYPE) {
        // copy header
        memcpy(target, &pdu->header, sizeof(mip_header));
        // copy sdu
        memcpy(target + sizeof(mip_header), pdu->sdu, sizeof(mip_arp_sdu));
        return;
    }
    printf("Invalid sdu type\n");
    exit(EXIT_FAILURE);
}

void deserialize_mip_pdu(mip_pdu* target, const uint8_t* serial_pdu) {
    target->header = *(mip_header*)(serial_pdu);
    target->sdu = malloc(target->header.sdu_len * 4);
    if (target->header.sdu_type == PING_SDU_TYPE) {
        deserialize_mip_ping_sdu(target->sdu, serial_pdu + sizeof(mip_header));
    } if (target->header.sdu_type == ARP_SDU_TYPE) {
        target->sdu = (mip_arp_sdu*)(serial_pdu + sizeof(mip_header));
    }
}

void free_mip_pdu(mip_pdu* pdu) {
    if (pdu->header.sdu_type == PING_SDU_TYPE) {
        free(((mip_ping_sdu*)pdu->sdu)->message);
        free(pdu->sdu);
    }
}

// Function to print mip_ping_sdu
void print_mip_ping_sdu(const mip_ping_sdu* sdu, int indent) {
    printf("%*sMIP Address: 0x%x\n", indent, "", sdu->mip_address);
    printf("%*sMessage: %s\n", indent, "", sdu->message);
}

// Function to print mip_arp_sdu
void print_mip_arp_sdu(const mip_arp_sdu* sdu, int indent) {
    if (sdu->type == ARP_REQUEST_TYPE) {
        printf("%*sType: %s\n", indent, "", "ARP_REQUEST_TYPE");
    } else if (sdu->type == ARP_RESPONSE_TYPE) {
        printf("%*sType: %s\n", indent, "", "ARP_RESPONSE_TYPE");
    } else {
        printf("%*sType: %s\n", indent, "", "Unknown arp type");
    }
    printf("%*sMIP Address: 0x%x\n", indent, "", sdu->mip_address);
}

// Function to print mip_header
void print_mip_header(const mip_header* header, int indent) {
    printf("%*sDest Address: 0x%x\n", indent, "", header->dest_address);
    printf("%*sSource Address: 0x%x\n", indent, "", header->source_address);
    printf("%*sTTL: %u\n", indent, "", header->ttl);
    printf("%*sSDU Length: %u\n", indent, "", header->sdu_len);
    if (header->sdu_type == PING_SDU_TYPE) {
        printf("%*sSDU Type: %s\n", indent, "", "PING_SDU_TYPE");
    } else if (header->sdu_type == ARP_SDU_TYPE) {
        printf("%*sSDU Type: %s\n", indent, "", "ARP_SDU_TYPE");
    } else {
        printf("%*sSDU Type: %s\n", indent, "", "Unknown SDU Type");
    }
}

// Function to print mip_pdu
void print_mip_pdu(const mip_pdu* pdu, int indent) {
    print_mip_header(&pdu->header, indent);
    printf("%*sSDU:\n", indent, "");
    switch (pdu->header.sdu_type) {
        case PING_SDU_TYPE:
            print_mip_ping_sdu(pdu->sdu, indent + 4);
        break;
        case ARP_SDU_TYPE:
            print_mip_arp_sdu(pdu->sdu, indent + 4);
        break;
        default:
            printf("%*sUnknown SDU Type: 0x%x\n", indent + 4, "", pdu->header.sdu_type);
        break;
    }
}

void build_mip_pdu(mip_pdu* target, const void* sdu, uint8_t source_address, uint8_t dest_address, uint8_t ttl, uint8_t sdu_type) {
    if (sdu_type == PING_SDU_TYPE) {
        mip_ping_sdu* ping_sdu = (mip_ping_sdu*) sdu;
        target->header.dest_address = dest_address;
        target->header.source_address = source_address;
        target->header.ttl = ttl;
        target->header.sdu_type = PING_SDU_TYPE;

        // + 1 because we include null-terminator in sdu_len
        // / 4 because it counts 32 bit words
        // + 3 to round up to nearest 4 bytes
        target->header.sdu_len = ((sizeof(ping_sdu->mip_address) + strlen(ping_sdu->message) + 1) + 3) / 4;
        target->sdu = ping_sdu;
    }
    else if (sdu_type == ARP_SDU_TYPE) {
        mip_arp_sdu* arp_sdu = (mip_arp_sdu*) sdu;

        if (arp_sdu->type == ARP_REQUEST_TYPE) {
            // broadcast mip address
            target->header.dest_address = 0xFF;
        } else {
            // Packet is arp response
            target->header.dest_address = dest_address;
        }
        target->header.source_address = source_address;
        target->header.ttl = ttl;
        target->header.sdu_type = ARP_SDU_TYPE;
        target->header.sdu_len = sizeof(*arp_sdu) / 4;

        target->sdu = arp_sdu;
    }
}