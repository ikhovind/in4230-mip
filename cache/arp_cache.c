#include <string.h>
#include <stdlib.h> /* standard library definitions (macros) */
#include <stdio.h>
#include "arp_cache.h"
#include "../packet_builder/eth_builder.h"


ArpCacheIndex* get_mac_address(ArpCache* cache, uint8_t mip_addr) {
    for (uint8_t i = 0; i < cache->map_length; ++i) {
        if (cache->cache_array[i].mip_addr == mip_addr) {
            return &cache->cache_array[i];
        }
    }
    return NULL;
}

bool insert_cache_index(ArpCache* cache, uint8_t mip_addr, uint8_t *mac_address, struct sockaddr_ll ll_addr) {

    if (get_mac_address(cache, mip_addr) != NULL) {
        // MIP address already in cache
        return false;
    }
    // Increase size of cache_array by 1, would be more efficient to double, but this is simpler
    ArpCacheIndex* new_cache_array = realloc(cache->cache_array, (cache->map_length + 1) * sizeof(ArpCacheIndex));

    if (!new_cache_array) {
        // Handle memory allocation failure
        fprintf(stderr, "Error reallocating memory.\n");
        return false;
    }

    new_cache_array[cache->map_length] = (ArpCacheIndex) {
        .mip_addr = mip_addr,
        .ll_addr = ll_addr
    };
    memcpy(new_cache_array[cache->map_length].mac_address, mac_address, 6 * sizeof(uint8_t));

    cache->cache_array = new_cache_array;
    cache->map_length += 1;
    return true;
}

// Function to print sockaddr_ll
void print_sockaddr_ll(struct sockaddr_ll ll_addr, int indent) {
    printf("%*sAddress Family: %u\n", indent, "", ll_addr.sll_family);
    printf("%*sProtocol: 0x%04x\n", indent, "", ll_addr.sll_protocol);
    printf("%*sInterface Index: 0x%x\n", indent, "", ll_addr.sll_ifindex);
    printf("%*sHardware Type: 0x%04x\n", indent, "", ll_addr.sll_hatype);
    printf("%*sPacket Type: %d\n", indent, "", ll_addr.sll_pkttype);
    printf("%*sAddress Length: %d\n", indent, "", ll_addr.sll_halen);
    print_mac_address("Address", ll_addr.sll_addr, indent + 4);
}

// Function to print ArpCacheIndex
void print_arp_cache_index(ArpCacheIndex index, int indent) {
    printf("%*sMIP Address: 0x%02x\n", indent, "", index.mip_addr);
    print_mac_address("MAC Address", index.mac_address, indent);
    printf("%*sLink-Layer Address:\n", indent, "");
    print_sockaddr_ll(index.ll_addr, indent + 4);
}

// Function to print ArpCache
void print_arp_cache(ArpCache* cache, int indent) {
    printf("%*sCurrent state of arp cache is:\n", indent, "");
    printf("%*sARP Cache Length: %u\n", indent + 4, "", cache->map_length);
    for (uint8_t i = 0; i < cache->map_length; ++i) {
        printf("%*sEntry %u:\n", indent + 4, "", i);
        print_arp_cache_index(cache->cache_array[i], indent + 8);
    }
}
