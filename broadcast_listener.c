//
// Created by ikhovind on 25.09.24.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/if_ether.h>  // for ETH_P_ALL
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>            // for struct ifreq
#include <unistd.h>
#include <errno.h>
#include <linux/if_packet.h>   // for struct sockaddr_ll
#include <linux/if_ether.h>    // for ETH_FRAME_LEN

#define INTERFACE "h2-eth1"

int main() {
    // Create a raw socket to listen for Ethernet frames
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the interface
    struct ifreq ifr;
    strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("Failed to get interface index");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_ll sll = {0};
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        perror("Binding failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Listening for broadcast packets on %s...\n", INTERFACE);

    unsigned char buffer[ETH_FRAME_LEN];
    while (1) {
        ssize_t num_bytes = recvfrom(sockfd, buffer, ETH_FRAME_LEN, 0, NULL, NULL);
        if (num_bytes == -1) {
            perror("Failed to receive packets");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Print the received packet information
        printf("Received packet %zd bytes\n", num_bytes);
        printf("Destination MAC: ");
        for (int i = 0; i < 6; i++) {
            printf("%02x", buffer[i]);
            if (i < 5) {
                printf(":");
            }
        }
        printf("\n");

        printf("Source MAC: ");
        for (int i = 0; i < 6; i++) {
            printf("%02x", buffer[i + 6]);
            if (i < 5) {
                printf(":");
            }
        }
        printf("\n");

        printf("Payload: ");
        for (int i = 14; i < num_bytes; i++) {
            printf("%02x", buffer[i]);
            if (i % 16 == 15) {
                printf("\n");
            } else {
                printf(" ");
            }
        }
        printf("\n\n");
    }

    close(sockfd);
    return 0;
}
