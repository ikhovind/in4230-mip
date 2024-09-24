CC = gcc
CFLAGS = -std=c11 -I./include

all: mipd ping_client

ping_client: ./network_interface/network_util.c
	$(CC) $(CFLAGS) -o ping_client chat.h chat.c ./network_interface/network_util.c ./client/ping_client.c 

mipd: ./mip_daemon/mip_daemon.c ./mip_daemon/mip_daemon.h
	$(CC) $(CFLAGS) -o mipd ./mip_daemon/mip_daemon.c ./mip_daemon/mip_daemon.h

ping_server: server ./network_interface/network_util.c
	$(CC) $(CFLAGS) -o ping_server ./ping_server/ping_server.c ./network_interface/network_util.c

clean:
	rm -f hjemmeeksamen1 mipd ping_client ping_server receiver sender main
