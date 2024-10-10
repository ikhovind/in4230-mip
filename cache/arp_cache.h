
#ifndef ARP_CACHE_H 
#define ARP_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <linux/if_packet.h>

typedef struct {
    uint8_t mip_addr;
    uint8_t mac_address[6];
    struct sockaddr_ll ll_addr;
} arp_cache_index;

typedef struct {
    uint8_t map_length;
    arp_cache_index* cache_array;
} arp_cache;



arp_cache_index* get_mac_address(arp_cache* cache, uint8_t mip_addr);
bool insert_cache_index(arp_cache* cache, uint8_t mip_addr, uint8_t* mac_address, struct sockaddr_ll ll_addr);
void print_arp_cache(arp_cache* cache, int indent);
void print_sockaddr_ll(struct sockaddr_ll ll_addr, int indent);



#endif //ARP_CACHE_H
