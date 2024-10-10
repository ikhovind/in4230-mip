//
// Created by ikhovind on 24.09.24.
//
#ifndef MIP_BUILDER_H
#define MIP_BUILDER_H

#include <stdint.h>

#define PING_SDU_TYPE 0x02
#define ARP_SDU_TYPE 0x01
#define ARP_REQUEST_TYPE 0x0
#define ARP_RESPONSE_TYPE 0x1
// constrained by length field in header: 2 ^ 9 - 1 = 511
#define MIP_SDU_MAX_LENGTH 511
// due to packing (?)

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
    uint16_t sdu_len: 9; // length including null-terminator in potential ping message
    uint8_t sdu_type: 3;
} mip_header;

typedef struct {
    mip_header header;
    void* sdu;
} mip_pdu;

enum {
    MIP_HEADER_SIZE = sizeof(mip_header),

    MIP_ARP_SDU_SIZE = sizeof(mip_arp_sdu),
    MIP_PING_SDU_MAX_SIZE = sizeof(mip_ping_sdu) + MIP_SDU_MAX_LENGTH,

    MIP_ARP_PDU_SIZE = MIP_HEADER_SIZE + MIP_ARP_SDU_SIZE,
    MIP_PDU_MAX_SIZE = MIP_HEADER_SIZE + MIP_SDU_MAX_LENGTH,
};

/**
* Serialize a mip_ping_sdu struct to a byte array
*
* @param target The byte array to serialize to (must be allocated by the caller to a size of: 1 + strlen(sdu->message) + 1)
* @param sdu The mip_ping_sdu struct to serialize. sdu-> message must contain a null-terminated string.
*/
void serialize_mip_ping_sdu(uint8_t* target, mip_ping_sdu* sdu);

/**
* Deserialize a mip_ping_sdu struct from a byte array
*
* @param target The mip_ping_sdu struct to deserialize to (must be allocated by the caller).
*         The message field will be allocated by the function and must be freed by the caller.
*         The message field will be null-terminated.
* @param buffer The byte array to deserialize from. as formatted by serialize_mip_ping_sdu.
*/
void deserialize_mip_ping_sdu(mip_ping_sdu* target, uint8_t* buffer);

/**
* Serialize a mip_arp_sdu struct to a byte array
*
* @param target The byte array to serialize to (must be allocated by the caller to a size of: sizeof(mip_header) + pdu->header->sdu_len)
* @param pdu The mip_pdu struct to serialize.
*      if pdu->header->sdu_type is ARP_SDU_TYPE, pdu->sdu must be a mip_arp_sdu struct.
*      if pdu->header->sdu_type is PING_SDU_TYPE, pdu->sdu must be a mip_ping_sdu struct.
*      if pdu->sdu is a mip_ping_sdu struct, the message field must contain a null-terminated string.
*/
void serialize_mip_pdu(uint8_t* target, mip_pdu* pdu);

/**
* Deserialize a mip_pdu struct from a byte array
*
* @param target The mip_pdu struct to deserialize to (must be allocated by the caller).
*         The sdu field will be allocated by the function to a size of pdu->header->sdu_len and must be freed by the caller.
*
* @param serial_pdu The byte array to deserialize from. as formatted by serialize_mip_pdu.
*/
void deserialize_mip_pdu(mip_pdu* target, uint8_t* serial_pdu);

/**
* build a mip_pdu struct
*
* @param target The output mip_pdu must be initialized by the caller.
* @param sdu The sdu to include in the pdu.
*      If sdu_type is PING_SDU_TYPE, sdu must be a mip_ping_sdu struct.
*      If sdu_type is ARP_SDU_TYPE, sdu must be a mip_arp_sdu struct.
* @param source_address The MIP address of the source node.
* @param dest_address The MIP address of the destination node.
* @param ttl Time To Live; maximum hop count
* @param sdu_type The type of the sdu. Must be either PING_SDU_TYPE or ARP_SDU_TYPE.
*/
void build_mip_pdu(mip_pdu* target, void* sdu, uint8_t source_address, uint8_t dest_address, uint8_t ttl, uint8_t type);

/**
 * Print a mip_arp_sdu struct to stdout
 */
void print_mip_pdu(mip_pdu* pdu, int indent);
/**
* Print a mip_arp_sdu struct to stdout
*/
void print_mip_arp_sdu(mip_arp_sdu* sdu, int indent);

// Function to print mip_ping_sdu
void print_mip_ping_sdu(mip_ping_sdu* sdu, int indent);

// Function to print mip_arp_sdu
void print_mip_arp_sdu(mip_arp_sdu* sdu, int indent);

// Function to print mip_header
void print_mip_header(mip_header* header, int indent);

#endif //MIP_BUILDER_H
