
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


/**
 * @brief Get the MAC address of a MIP address from the ARP cache.
 * @param cache The ARP cache.
 * @param mip_addr The MIP address to look up.
 * @return Pointer to the element in the cache matching given MIP address,
 *         NULL if the MIP address is not in the cache.
 */
ArpCacheIndex* get_mac_address(ArpCache* cache, uint8_t mip_addr);

/**
 * @brief Insert a new entry into the ARP cache.
 *
 * Reallocates the cache array to fit the new entry.
 * cache_array must be freed by the caller when the cache is no longer needed.
 *
 * @param cache The ARP cache.
 * @param mip_addr The MIP address to insert.
 * @param mac_address The MAC address to insert.
 * @param ll_addr The link-layer address to insert.
 * @return True if the insertion was successful, false otherwise.
 */
bool insert_cache_index(ArpCache* cache, uint8_t mip_addr, uint8_t* mac_address, struct sockaddr_ll ll_addr);

/**
* @brief Print an ArpCache in a human-readable format.
*
* @param cache The ARP cache to print.
* @param indent Base indentation level of output.
*/
void print_arp_cache(ArpCache* cache, int indent);

/**
* @brief Print an sockaddr_ll in a human-readable format.
* @param ll_addr The sockaddr_ll to print.
* @param indent Base indentation level of output.
*/
void print_sockaddr_ll(struct sockaddr_ll ll_addr, int indent);



#endif //ARP_CACHE_H
