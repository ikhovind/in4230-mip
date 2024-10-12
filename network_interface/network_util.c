#define _DEFAULT_SOURCE
#include "network_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_packet.h>  // for struct sockaddr_ll
#include <net/if.h>
#include <sys/ioctl.h>

void get_interface_mac_address(unsigned char local_mac[6], const int sll_ifindex, const int raw_sd) {
	// Convert interface index to name
	char if_name[IFNAMSIZ];
	if (if_indextoname(sll_ifindex, if_name) == NULL) {
		perror("if_indextoname");
		close(raw_sd);
		exit(EXIT_FAILURE);
	}

	// Get your own (local) MAC address
	struct ifreq ifr;
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ - 1);
	ifr.ifr_name[IFNAMSIZ - 1] = '\0';

	if (ioctl(raw_sd, SIOCGIFHWADDR, &ifr) < 0) {
		perror("ioctl SIOCGIFHWADDR");
		close(raw_sd);
		exit(EXIT_FAILURE);
	}
	memcpy(local_mac, ifr.ifr_hwaddr.sa_data, 6);
}

