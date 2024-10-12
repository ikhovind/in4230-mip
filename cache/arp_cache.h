
#ifndef ARP_CACHE_H 
#define ARP_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/if_packet.h>

typedef struct {
    uint8_t mip_addr;
    uint8_t mac_address[6];
    struct sockaddr_ll ll_addr;
} ArpCacheIndex;

typedef struct {
    uint8_t map_length;
    ArpCacheIndex* cache_array;
} ArpCache;



ArpCacheIndex* get_mac_address(ArpCache* cache, uint8_t mip_addr);
bool insert_cache_index(ArpCache* cache, uint8_t mip_addr, uint8_t* mac_address, struct sockaddr_ll ll_addr);
void print_arp_cache(ArpCache* cache, int indent);
void print_sockaddr_ll(struct sockaddr_ll ll_addr, int indent);



#endif //ARP_CACHE_H
