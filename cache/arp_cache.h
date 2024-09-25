
#ifndef ARP_CACHE_H 
#define ARP_CACHE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t mip_addr;
    uint8_t mac_address[6];
} arp_cache_index;

typedef struct {
    uint8_t map_length;
    arp_cache_index* cache_array;
} arp_cache;



uint8_t* get_mac_address(arp_cache* cache, uint8_t mip_addr);
bool insert_cache_index(arp_cache* cache, uint8_t mip_addr, uint8_t* mac_address);



#endif //ARP_CACHE_H
