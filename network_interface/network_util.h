
#ifndef NETWORK_UTIL_H
#define NETWORK_UTIL_H

/**
 * Get the MAC address of the interface with the given index.
 * @param local_mac[out] The MAC address of the interface.
 * @param sll_ifindex The index of the interface.
 * @param raw_sd The raw socket descriptor for talking to interface.
 *
 * @note Exits the program if getting the hardware address fails or getting the interface name fails.
 */
void get_interface_mac_address(unsigned char local_mac[6], int sll_ifindex, int raw_sd);


#endif //NETWORK_UTIL_H
