#ifndef ETH_BUILDER_H
#define ETH_BUILDER_H
#include <stdint.h>

#include "mip_builder.h"

#define ETH_ADDR_LEN 6
#define ETH_TYPE_LEN 2
#define ETH_HEADER_LEN (ETH_ADDR_LEN * 2 + ETH_TYPE_LEN)

#define ETH_P_MIP 0x88b5

typedef struct {
    uint8_t dest_address[ETH_ADDR_LEN];
    uint8_t source_address[ETH_ADDR_LEN];
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

/**
 * @brief Build an Ethernet header
 *
 * @param header[out]: Pointer to the header to build
 * @param dest_address: Destination MAC address
 * @param source_address: Source MAC address
 * @param ethertype: Ethertype, for MIP use ETH_P_MIP
 */
void build_eth_header(eth_header* header, const uint8_t dest_address[ETH_ADDR_LEN], const uint8_t source_address[ETH_ADDR_LEN], uint16_t ethertype);

/**
 * @brief Build an Ethernet PDU
 *
 * @param pdu[out]: Pointer to the PDU to build
 * @param header: Ethernet header to copy to the ETH PDU
 * @param mip_pdu: MIP PDU to copy to the ETH PDU
 */
void build_eth_pdu(eth_pdu* pdu, const eth_header* header, const mip_pdu* mip_pdu);

/**
 * @brief Serialize an Ethernet PDU
 *
 * @param target[out]: Pointer to the buffer to serialize to
 * @param pdu: Ethernet PDU to serialize
 */
void serialize_eth_pdu(uint8_t* target, const eth_pdu* pdu);

/**
 * @brief Deserialize an Ethernet PDU
 *
 * This function will invoce deserialize_mip_pdu, this function will allocate memory for target->mip_pdu.sdu,
 * which must be freed by the caller.
 *
 * If the contained MIP PDU is a ping sdu, this function will invoke deserialize_mip_ping_sdu, this function will
 * allocate memory for target->mip_pdu.sdu->message, which must be freed by the caller.
 *
 * This can be done by using free_mip_pdu.
 *
 * @param target[out]: Pointer to the Ethernet PDU to deserialize to
 * @param buffer: Buffer to deserialize from
 */
void deserialize_eth_pdu(eth_pdu* target, const uint8_t* buffer);

/**
 * @brief Print a MAC address
 *
 * @param label: Label to print before the MAC address
 * @param address: MAC address to print
 * @param indent Base indentation level of output.
 */
void print_mac_address(const char *label, const uint8_t address[ETH_ADDR_LEN], int indent);

/**
 * @brief Print an Ethernet header
 *
 * @param header: Ethernet header to print
 * @param indent Base indentation level of output.
 */
void print_eth_header(const eth_header* header, int indent);

/**
 * @brief Print an Ethernet PDU
 *
 * @param pdu: Ethernet PDU to print
 * @param indent Base indentation level of output.
 */
void print_eth_pdu(const eth_pdu* pdu, int indent);



#endif //ETH_BUILDER_H