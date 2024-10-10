#include "Unity/src/unity.h"
#include "../packet_builder/mip_builder.h"
#include "../packet_builder/eth_builder.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_serialize_mip_ping_sdu(void) {
    /*
    * Format should be (total length can be up to 2^9 - 1 bytes because length in pdu header is 9 bits):
    * +--------------------------+-------------------------------------------------+
    * | mip_address 8 bit        | null-terminated message (up to 2^9 - 2 bytes)   |
    * +--------------------------+-------------------------------------------------+
    */
    uint8_t serial_buf[256];
    mip_ping_sdu sdu;

    sdu.message = "hello";
    sdu.mip_address = 200;

    // then serialize
    serialize_mip_ping_sdu(serial_buf, &sdu);

    TEST_ASSERT_EQUAL(200, serial_buf[0]);
    TEST_ASSERT_EQUAL_STRING("hello", &serial_buf[1]);
}

void test_deserialize_mip_ping_sdu(void) {
    uint8_t* serial_sdu = malloc(1 + 1 + strlen("hello") + 1);
    serial_sdu[0] = 200;
    memcpy(&serial_sdu[1], "hello", 5);
    serial_sdu[1 + 5] = '\0';

    mip_ping_sdu* ping_sdu = malloc(sizeof(uint8_t));
    deserialize_mip_ping_sdu(ping_sdu, serial_sdu);
    TEST_ASSERT_EQUAL(200, ping_sdu->mip_address);
    TEST_ASSERT_EQUAL_STRING("hello", ping_sdu->message);

    free(serial_sdu);
}

void test_serialize_mip_ping(void) {
    // first build our PDU
    mip_pdu* arp_pdu = malloc(256);
    mip_ping_sdu sdu;
    sdu.mip_address = 200;
    sdu.message = "Can I be serialized?";

    sdu.mip_address = 200;
    build_mip_pdu(arp_pdu, &sdu, 100, 200, 1, PING_SDU_TYPE);

    uint8_t serialized[256];
    // then serialize and deserialize it
    serialize_mip_pdu(serialized, arp_pdu);

    mip_pdu* deserialized_pdu = malloc(sizeof(mip_header));

    deserialize_mip_pdu(deserialized_pdu, serialized);

    // check that content of deserialized pdu is the same as the serialized one
    TEST_ASSERT_EQUAL_PTR(sdu.mip_address, ((mip_ping_sdu*)deserialized_pdu->sdu)->mip_address);
    TEST_ASSERT_EQUAL_STRING(sdu.message, ((mip_ping_sdu*)deserialized_pdu->sdu)->message);

    TEST_ASSERT_EQUAL(arp_pdu->header.dest_address, deserialized_pdu->header.dest_address);
    TEST_ASSERT_EQUAL(arp_pdu->header.source_address, deserialized_pdu->header.source_address);
    TEST_ASSERT_EQUAL(arp_pdu->header.ttl, deserialized_pdu->header.ttl);
    TEST_ASSERT_EQUAL(arp_pdu->header.sdu_len, deserialized_pdu->header.sdu_len);
    TEST_ASSERT_EQUAL(arp_pdu->header.sdu_type, deserialized_pdu->header.sdu_type);
}

void test_serialize_mip_arp(void) {
    // first build our PDU
    mip_pdu arp_pdu;
    mip_arp_sdu sdu;

    sdu.type = ARP_REQUEST_TYPE;
    sdu.mip_address = 200;
    build_mip_pdu(&arp_pdu, &sdu, 100, 200, 1, ARP_SDU_TYPE);

    // then serialize and deserialize it
    uint8_t serialized[256];
    serialize_mip_pdu(serialized, &arp_pdu);

    mip_pdu* deserialized_pdu = malloc(sizeof(mip_header));

    deserialize_mip_pdu(deserialized_pdu, serialized);

    // check that content of deserialized pdu is the same as the serialized one
    TEST_ASSERT_EQUAL(sdu.type, ((mip_arp_sdu*)deserialized_pdu->sdu)->type);
    TEST_ASSERT_EQUAL(sdu.mip_address, ((mip_arp_sdu*)deserialized_pdu->sdu)->mip_address);
    TEST_ASSERT_EQUAL(sdu.padding, ((mip_arp_sdu*)deserialized_pdu->sdu)->padding);
    TEST_ASSERT_EQUAL(arp_pdu.header.dest_address, deserialized_pdu->header.dest_address);
    TEST_ASSERT_EQUAL(arp_pdu.header.source_address, deserialized_pdu->header.source_address);
    TEST_ASSERT_EQUAL(arp_pdu.header.ttl, deserialized_pdu->header.ttl);
    TEST_ASSERT_EQUAL(arp_pdu.header.sdu_len, deserialized_pdu->header.sdu_len);
    TEST_ASSERT_EQUAL(arp_pdu.header.sdu_type, deserialized_pdu->header.sdu_type);
}

void test_serialize_eth_pdu(void) {
    eth_pdu pdu;
    eth_header header;
    mip_pdu mipPdu;
    mip_ping_sdu sdu;

    sdu.mip_address = 99;
    sdu.message = "hello";

    uint8_t dest_address[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t source_address[6] = {0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
    uint16_t ethertype = ETH_MIP_TYPE;

    build_eth_header(&header, dest_address, source_address, 0x88b);
    build_mip_pdu(&mipPdu, &sdu, 100, 200, 1, PING_SDU_TYPE);
    build_eth_pdu(&pdu, &header, &mipPdu);

    uint8_t serialized[ETH_PDU_MAX_SIZE];
    serialize_eth_pdu(serialized, &pdu);

    eth_pdu deserialized_eth_pdu;
    deserialize_eth_pdu(&deserialized_eth_pdu, serialized);

    // check that content of deserialized pdu is the same as the serialized one
    TEST_ASSERT_EQUAL_MEMORY(header.dest_address, deserialized_eth_pdu.header.dest_address, ETH_ADDR_LEN);
    TEST_ASSERT_EQUAL_MEMORY(header.source_address, deserialized_eth_pdu.header.source_address, ETH_ADDR_LEN);
   	TEST_ASSERT_EQUAL(header.ethertype, deserialized_eth_pdu.header.ethertype);

    mip_pdu deserialized_mip_pdu = deserialized_eth_pdu.mip_pdu;
    TEST_ASSERT_EQUAL(mipPdu.header.dest_address, deserialized_mip_pdu.header.dest_address);
    TEST_ASSERT_EQUAL(mipPdu.header.source_address, deserialized_mip_pdu.header.source_address);
    TEST_ASSERT_EQUAL(mipPdu.header.ttl, deserialized_mip_pdu.header.ttl);
    TEST_ASSERT_EQUAL(mipPdu.header.sdu_len, deserialized_mip_pdu.header.sdu_len);
    TEST_ASSERT_EQUAL(PING_SDU_TYPE, deserialized_mip_pdu.header.sdu_type);

    mip_ping_sdu* deserialized_sdu = (mip_ping_sdu*)deserialized_mip_pdu.sdu;
    TEST_ASSERT_EQUAL(sdu.mip_address, deserialized_sdu->mip_address);
    TEST_ASSERT_EQUAL_STRING(sdu.message, deserialized_sdu->message);
}

void test_serialize_eth_pdu_with_arp_mip() {
    eth_pdu pdu;
    eth_header header;
    mip_pdu mipPdu;
    mip_arp_sdu sdu;

    sdu.type = 1;
    sdu.mip_address = 99;

    uint8_t dest_address[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t source_address[6] = {0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
    uint16_t ethertype = ETH_MIP_TYPE;

    build_eth_header(&header, dest_address, source_address, 0x88b);
    build_mip_pdu(&mipPdu, &sdu, 100, 200, 1, ARP_SDU_TYPE);
    build_eth_pdu(&pdu, &header, &mipPdu);

    uint8_t serialized[ETH_PDU_MAX_SIZE];
    serialize_eth_pdu(serialized, &pdu);

    eth_pdu deserialized_eth_pdu;
    deserialize_eth_pdu(&deserialized_eth_pdu, serialized);

    // check that content of deserialized pdu is the same as the serialized one
    TEST_ASSERT_EQUAL_MEMORY(header.dest_address, deserialized_eth_pdu.header.dest_address, ETH_ADDR_LEN);
    TEST_ASSERT_EQUAL_MEMORY(header.source_address, deserialized_eth_pdu.header.source_address, ETH_ADDR_LEN);
   	TEST_ASSERT_EQUAL(header.ethertype, deserialized_eth_pdu.header.ethertype);

    mip_pdu deserialized_mip_pdu = deserialized_eth_pdu.mip_pdu;
    TEST_ASSERT_EQUAL(mipPdu.header.dest_address, deserialized_mip_pdu.header.dest_address);
    TEST_ASSERT_EQUAL(mipPdu.header.source_address, deserialized_mip_pdu.header.source_address);
    TEST_ASSERT_EQUAL(mipPdu.header.ttl, deserialized_mip_pdu.header.ttl);
    TEST_ASSERT_EQUAL(mipPdu.header.sdu_len, deserialized_mip_pdu.header.sdu_len);
    TEST_ASSERT_EQUAL(ARP_SDU_TYPE, deserialized_mip_pdu.header.sdu_type);

    mip_arp_sdu* deserialized_sdu = (mip_arp_sdu*)deserialized_mip_pdu.sdu;
    TEST_ASSERT_EQUAL(sdu.mip_address, deserialized_sdu->mip_address);
    TEST_ASSERT_EQUAL(sdu.type, deserialized_sdu->type);
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_mip_arp);
    RUN_TEST(test_serialize_mip_ping);
    RUN_TEST(test_serialize_mip_ping_sdu);
    RUN_TEST(test_deserialize_mip_ping_sdu);
    RUN_TEST(test_serialize_eth_pdu);
    RUN_TEST(test_serialize_eth_pdu_with_arp_mip);
    return UNITY_END();
}
