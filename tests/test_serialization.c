#include "Unity/src/unity.h"
#include "../packet_builder/mip_builder.h"
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
    * Format should be
    * +--------------------------+-------------------+---------------------------+
    * | size            (8 bit)  | mip_address 8 bit | message (up to 253 bytes) |
    * +--------------------------+-------------------+---------------------------+
    * |
    *
    *
    */
    mip_ping_sdu sdu;
    sdu.mip_address = 200;

    sdu.message = "hello";
    sdu.mip_address = 200;

    size_t size = 0;
    // then serialize and deserialize it
    uint8_t* serialized = serialize_mip_ping_sdu(&sdu, &size);

    TEST_ASSERT_EQUAL(7, serialized[0]);
    TEST_ASSERT_EQUAL(200, serialized[1]);
    TEST_ASSERT_EQUAL_STRING("hello", &serialized[2]);
}

void test_deserialize_mip_ping_sdu(void) {
    uint8_t* serial_sdu = malloc(1 + 1 + strlen("hello") + 1);
    serial_sdu[0] = 7;
    serial_sdu[1] = 200;
    memcpy(&serial_sdu[2], "hello", 5);
    serial_sdu[2 + 5] = '\0';

    mip_ping_sdu* ping_sdu = deserialize_mip_ping_sdu(serial_sdu);
    TEST_ASSERT_EQUAL(200, ping_sdu->mip_address);
    TEST_ASSERT_EQUAL_STRING("hello", ping_sdu->message);
}

void test_serialize_mip_ping(void) {
    // first build our PDU
    mip_pdu arp_pdu;
    mip_ping_sdu sdu;
    sdu.mip_address = 200;
    sdu.message = "Can I be serialized?";

    sdu.mip_address = 200;
    build_mip_pdu(&arp_pdu, &sdu, 100, 200, 1, PING_SDU_TYPE);

    size_t size = 0;
    // then serialize and deserialize it
    uint8_t* serialized = serialize_mip_pdu(&arp_pdu, &size);

    mip_pdu deserialized_pdu;
    deserialize_mip_pdu(&deserialized_pdu, serialized);

    // check that content of deserialized pdu is the same as the serialized one
    TEST_ASSERT_EQUAL_PTR(sdu.mip_address, ((mip_ping_sdu*)deserialized_pdu.sdu)->mip_address);
    TEST_ASSERT_EQUAL_STRING(sdu.message, ((mip_ping_sdu*)deserialized_pdu.sdu)->message);

    TEST_ASSERT_EQUAL(arp_pdu.header.dest_address, deserialized_pdu.header.dest_address);
    TEST_ASSERT_EQUAL(arp_pdu.header.source_address, deserialized_pdu.header.source_address);
    TEST_ASSERT_EQUAL(arp_pdu.header.ttl, deserialized_pdu.header.ttl);
    TEST_ASSERT_EQUAL(arp_pdu.header.sdu_len, deserialized_pdu.header.sdu_len);
    TEST_ASSERT_EQUAL(arp_pdu.header.sdu_type, deserialized_pdu.header.sdu_type);
}

void test_serialize_mip_arp(void) {
    // first build our PDU
    mip_pdu arp_pdu;
    mip_arp_sdu sdu;

    sdu.type = ARP_REQUEST_TYPE;
    sdu.mip_address = 200;
    build_mip_pdu(&arp_pdu, &sdu, 100, 200, 1, ARP_SDU_TYPE);

    size_t size = 0;
    // then serialize and deserialize it
    uint8_t* serialized = serialize_mip_pdu(&arp_pdu, &size);

    mip_pdu deserialized_pdu;
    deserialize_mip_pdu(&deserialized_pdu, serialized);

    // check that content of deserialized pdu is the same as the serialized one
    TEST_ASSERT_EQUAL(sdu.type, ((mip_arp_sdu*)deserialized_pdu.sdu)->type);
    TEST_ASSERT_EQUAL(sdu.mip_address, ((mip_arp_sdu*)deserialized_pdu.sdu)->mip_address);
    TEST_ASSERT_EQUAL(sdu.padding, ((mip_arp_sdu*)deserialized_pdu.sdu)->padding);
    TEST_ASSERT_EQUAL(arp_pdu.header.dest_address, deserialized_pdu.header.dest_address);
    TEST_ASSERT_EQUAL(arp_pdu.header.source_address, deserialized_pdu.header.source_address);
    TEST_ASSERT_EQUAL(arp_pdu.header.ttl, deserialized_pdu.header.ttl);
    TEST_ASSERT_EQUAL(arp_pdu.header.sdu_len, deserialized_pdu.header.sdu_len);
    TEST_ASSERT_EQUAL(arp_pdu.header.sdu_type, deserialized_pdu.header.sdu_type);
}

// not needed when using generate_test_runner.rb
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_mip_arp);
    RUN_TEST(test_serialize_mip_ping);
    RUN_TEST(test_serialize_mip_ping_sdu);
    RUN_TEST(test_deserialize_mip_ping_sdu);
    return UNITY_END();
}
