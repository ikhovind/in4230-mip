//
// Created by ikhovind on 24.09.24.
//

#include "mip_builder.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void serialize_mip_ping_sdu(uint8_t* target, mip_ping_sdu* sdu) {
    // Allocate buffer for serialized data
    // Copy mip_address to buffer
    target[0] = sdu->mip_address;

    // Copy message to buffer after mip_address
    strcpy((char*) target + 1, sdu->message);
}

// function to deserialize mip_ping_sdu
void deserialize_mip_ping_sdu(mip_ping_sdu* target, uint8_t* buffer) {
    // Copy mip_address from buffer
    target->mip_address = buffer[0];

    // allocate memory for message and null-terminator
    target->message = malloc(strlen((char*)(buffer + 1)) + 1);
    strcpy(target->message, (char*)(buffer + 1));
}

void serialize_mip_pdu(uint8_t* target, mip_pdu* pdu) {
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

void deserialize_mip_pdu(mip_pdu* target, uint8_t* serial_pdu) {
    target->header = *(mip_header*)(serial_pdu);
    target->sdu = malloc(target->header.sdu_len);
    if (target->header.sdu_type == PING_SDU_TYPE) {
        deserialize_mip_ping_sdu(target->sdu, serial_pdu + sizeof(mip_header));
    } if (target->header.sdu_type == ARP_SDU_TYPE) {
        target->sdu = (mip_arp_sdu*)(serial_pdu + sizeof(mip_header));
    }
}

// Function to print mip_ping_sdu
void print_mip_ping_sdu(mip_ping_sdu* sdu, int indent) {
    printf("%*sMIP Address: 0x%d\n", indent, "", sdu->mip_address);
    printf("%*sMessage: %s\n", indent, "", sdu->message);
}

// Function to print mip_arp_sdu
void print_mip_arp_sdu(mip_arp_sdu* sdu, int indent) {
    if (sdu->type == ARP_REQUEST_TYPE) {
        printf("%*sType: %s\n", indent, "", "ARP_REQUEST_TYPE");
    } else if (sdu->type == ARP_RESPONSE_TYPE) {
        printf("%*sType: %s\n", indent, "", "ARP_RESPONSE_TYPE");
    } else {
        printf("%*sType: %s\n", indent, "", "Unknown arp type");
    }
    printf("%*sMIP Address: %d\n", indent, "", sdu->mip_address);
}

// Function to print mip_header
void print_mip_header(mip_header* header, int indent) {
    printf("%*sDest Address: %d\n", indent, "", header->dest_address);
    printf("%*sSource Address: %d\n", indent, "", header->source_address);
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
void print_mip_pdu(mip_pdu* pdu, int indent) {
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

void build_mip_pdu(mip_pdu* target, void* sdu, uint8_t source_address, uint8_t dest_address, uint8_t ttl, uint8_t type) {
    // Fill the MIP header
    if (type == PING_SDU_TYPE) {
        mip_ping_sdu* ping_sdu = (mip_ping_sdu*) sdu;
        target->header.dest_address = dest_address;
        target->header.source_address = source_address;
        target->header.ttl = ttl;
        target->header.sdu_type = PING_SDU_TYPE;

        size_t sdu_len = sizeof(ping_sdu->mip_address) + strlen(ping_sdu->message) + 1;
        target->header.sdu_len = sdu_len;
        target->sdu = ping_sdu;
    }
    else if (type == ARP_SDU_TYPE) {
        mip_arp_sdu* arp_sdu = (mip_arp_sdu*) sdu;
        arp_sdu->type = ARP_REQUEST_TYPE;
        arp_sdu->mip_address = dest_address;

        target->header.dest_address = dest_address;
        target->header.source_address = source_address;
        target->header.ttl = ttl;
        target->header.sdu_type = ARP_SDU_TYPE;
        target->header.sdu_len = sizeof(*arp_sdu);

        target->sdu = arp_sdu;
    }
}

//
// Created by ikhovind on 24.09.24.
//
// Created by ikhovind on 24.09.24.
//
