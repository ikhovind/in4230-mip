# Compiler
CC = gcc
CFLAGS = -std=c11 -I./include

# Source files and their corresponding object files
SRC_MIP_BUILDER = ./packet_builder/mip_builder.c
OBJ_MIP_BUILDER = $(SRC_MIP_BUILDER:.c=.o)

SRC_ARP_CACHE = ./cache/arp_cache.c
OBJ_ARP_CACHE = $(SRC_ARP_CACHE:.c=.o)

SRC_NETWORK_UTIL = ./network_interface/network_util.c
OBJ_NETWORK_UTIL = $(SRC_NETWORK_UTIL:.c=.o)

SRC_CHAT = chat.c
OBJ_CHAT = $(SRC_CHAT:.c=.o)

SRC_PING_CLIENT = ./client/ping_client.c
OBJ_PING_CLIENT = $(SRC_PING_CLIENT:.c=.o)

SRC_MIP_DAEMON = ./mip_daemon/mip_daemon.c
OBJ_MIP_DAEMON = $(SRC_MIP_DAEMON:.c=.o)

SRC_PING_SERVER = ./server/ping_server.c
OBJ_PING_SERVER = $(SRC_PING_SERVER:.c=.o)

# Targets
TARGET_PING_CLIENT = ping_client
TARGET_MIP_DAEMON = mipd
TARGET_PING_SERVER = ping_server

# Default target
all: $(TARGET_MIP_DAEMON) $(TARGET_PING_CLIENT) $(TARGET_PING_SERVER)

# Rule to build ping_client
$(TARGET_PING_CLIENT): $(OBJ_ARP_CACHE) $(OBJ_MIP_BUILDER) $(OBJ_NETWORK_UTIL) $(OBJ_CHAT) $(OBJ_PING_CLIENT)
	$(CC) $(CFLAGS) -o $(TARGET_PING_CLIENT) $(OBJ_MIP_BUILDER) $(OBJ_ARP_CACHE) $(OBJ_NETWORK_UTIL) $(OBJ_CHAT) $(OBJ_PING_CLIENT)

# Rule to build mipd
$(TARGET_MIP_DAEMON): $(OBJ_MIP_DAEMON) $(OBJ_NETWORK_UTIL) $(OBJ_MIP_BUILDER) $(OBJ_ARP_CACHE)
	$(CC) $(CFLAGS) -o $(TARGET_MIP_DAEMON) $(OBJ_MIP_DAEMON) $(OBJ_NETWORK_UTIL) $(OBJ_MIP_BUILDER) $(OBJ_ARP_CACHE)

# Rule to build ping_server
$(TARGET_PING_SERVER): $(OBJ_NETWORK_UTIL) $(OBJ_PING_SERVER) $(OBJ_MIP_BUILDER)
	$(CC) $(CFLAGS) -o $(TARGET_PING_SERVER) $(OBJ_NETWORK_UTIL) $(OBJ_PING_SERVER) $(OBJ_MIP_BUILDER)

# Rule to build all .o files from their corresponding .c files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove generated files
clean:
	rm -f $(OBJ_ARP_CACHE) $(OBJ_NETWORK_UTIL) $(OBJ_CHAT) $(OBJ_PING_CLIENT) $(OBJ_MIP_DAEMON) $(OBJ_PING_SERVER)
	rm -f $(TARGET_MIP_DAEMON) $(TARGET_PING_CLIENT) $(TARGET_PING_SERVER)

.PHONY: all cleam
