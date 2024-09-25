#include <string.h>
#include <stdlib.h> /* standard library definitions (macros) */
#include <stdio.h>
#include "arp_cache.h"


uint8_t* get_mac_address(arp_cache* cache, uint8_t mip_addr) {
    for (uint8_t i = 0; i < cache->map_length; ++i) {
        if (cache->cache_array[i].mip_addr == mip_addr) {
            return cache->cache_array[i].mac_address;
        }
    }
    return NULL;
}

bool insert_cache_index(arp_cache* cache, uint8_t mip_addr, uint8_t *mac_address) {

    // should duble here but oh well
    arp_cache_index* new_cache_array = realloc(cache->cache_array, (cache->map_length + 1) * sizeof(arp_cache_index));

    if (!new_cache_array) {
        // Handle memory allocation failure
        fprintf(stderr, "Error reallocating memory.\n");
        return false;
    }

    new_cache_array[cache->map_length] = (arp_cache_index) {
        .mip_addr = mip_addr,
    };
    memcpy(new_cache_array[cache->map_length].mac_address, mac_address, 6 * sizeof(uint8_t));

    cache->cache_array = new_cache_array;
    cache->map_length += 1;
    return true;
}
