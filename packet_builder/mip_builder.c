//
// Created by ikhovind on 24.09.24.
//

#include "mip_builder.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

uint8_t* serialize_mip_ping_sdu(mip_ping_sdu* sdu, size_t* size) {
    // Calculate the size of the serialized data
    size_t message_len = strlen(sdu->message);
    *size = sizeof(sdu->mip_address) + message_len + 1;

    // Allocate buffer for serialized data
    uint8_t* buffer = malloc(*size);
    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    buffer[0] = *size;
    // Copy mip_address to buffer
    buffer[1] = sdu->mip_address;

    // Copy message to buffer, after size and mip_address
    memcpy(&buffer[2], (uint8_t *)sdu->message, *size - 2);

    return buffer;
}

// function to deserialize mip_ping_sdu
mip_ping_sdu* deserialize_mip_ping_sdu(uint8_t* buffer) {
    // Allocate memory for mip_ping_sdu
    mip_ping_sdu* sdu = (mip_ping_sdu*)malloc(sizeof(mip_ping_sdu));
    if (!sdu) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    uint8_t size = buffer[0];
    if (size == 0) {
        return NULL;
    }
    // Copy mip_address from buffer
    sdu->mip_address = buffer[1];

    // Copy message from buffer
    size_t message_len = size - sizeof(sdu->mip_address) - sizeof(size);;
    sdu->message = (char*)malloc(message_len + 1);
    if (!sdu->message) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memcpy(sdu->message, buffer + sizeof(size) + sizeof(sdu->mip_address), message_len);
    sdu->message[message_len] = '\0';

    return sdu;
}

uint8_t* serialize_mip_pdu(mip_pdu* pdu, size_t* size) {
    if (pdu->header.sdu_type == PING_SDU_TYPE) {
        return 0;
    }
    if (pdu->header.sdu_type == ARP_SDU_TYPE) {
        uint8_t* test = malloc(sizeof(mip_header) + sizeof(mip_arp_sdu));
        memcpy(test, &pdu->header, sizeof(mip_header));
        memcpy(test + sizeof(mip_header), pdu->sdu, sizeof(mip_arp_sdu));
        *size = sizeof(mip_header) + sizeof(mip_arp_sdu);
        return test;
    }
    printf("Invalid sdu type\n");
    exit(EXIT_FAILURE);
}

void deserialize_mip_pdu(mip_pdu* pdu, uint8_t* serial_pdu) {
    pdu->header = *(mip_header*)(serial_pdu + 14);
    if (pdu->header.sdu_type == PING_SDU_TYPE) {
        return;
    } if (pdu->header.sdu_type == ARP_SDU_TYPE) {
        pdu->sdu = (mip_arp_sdu*)(serial_pdu + sizeof(mip_header));
    }
}

void print_mip_ping_sdu(mip_ping_sdu* sdu) {
    if (!sdu) {
        return;
    }
    printf("ping SDU:\n");
    printf("{\n   mip_address: %d\n", sdu->mip_address);
    printf("   message: %s\n}\n", sdu->message);
}

void print_mip_arp_sdu(mip_arp_sdu* sdu) {
    printf("arp SDU:\n");
    printf("{\n   type: 0x%x\n", sdu->type);
    printf("   mip_address: %d\n}\n", sdu->mip_address);
}

void print_mip_pdu(mip_pdu* pdu) {
    if (!pdu) {
        return;
    }
    printf("mip PDU:\n");
    printf("{\n   dest_address: %d\n", pdu->header.dest_address);
    printf("   source_address: %d\n", pdu->header.source_address);
    printf("   ttl: %d\n", pdu->header.ttl);
    printf("   sdu_len: %d\n", pdu->header.sdu_len);
    printf("   sdu_type: %d\n", pdu->header.sdu_type);

    if (pdu->header.sdu_type == PING_SDU_TYPE) {
        print_mip_ping_sdu((mip_ping_sdu*)pdu->sdu);
    } else if (pdu->header.sdu_type == ARP_SDU_TYPE) {
        print_mip_arp_sdu((mip_arp_sdu*)pdu->sdu);
    }
    printf("}\n");
}

void build_mip_pdu(mip_pdu* pdu, void* sdu, uint8_t source_address, uint8_t ttl, uint8_t type) {
    // Fill the MIP header
    if (type == PING_SDU_TYPE) {
        mip_ping_sdu* ping_sdu = (mip_ping_sdu*) sdu;
        pdu->header.dest_address = ping_sdu->mip_address;
        pdu->header.source_address = source_address;
        pdu->header.ttl = ttl;
        pdu->header.sdu_type = PING_SDU_TYPE;

        size_t sdu_len = sizeof(ping_sdu->mip_address) + strlen(ping_sdu->message);
        pdu->header.sdu_len = sdu_len;
        pdu->sdu = ping_sdu;
    }
    else if (type == ARP_SDU_TYPE) {
        // currently only works for building request
        // can input dest_address and check if arp_sdu->mip_adress == source_address => it is a response
        mip_arp_sdu* arp_sdu = (mip_arp_sdu*) sdu;
        arp_sdu->type = ARP_REQUEST_TYPE;

        pdu->header.dest_address = arp_sdu->mip_address;
        pdu->header.source_address = source_address;
        pdu->header.ttl = ttl;
        pdu->header.sdu_type = ARP_SDU_TYPE;

        pdu->sdu = arp_sdu;
    }
}

//
// Created by ikhovind on 24.09.24.
//
// Created by ikhovind on 24.09.24.
//