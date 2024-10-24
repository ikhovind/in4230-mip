# Compiler
CC = gcc
CFLAGS = -std=c11 -I./include

# Source directories
SRC_DIRS := packet_builder cache network_interface client mip_daemon server
TEST_DIRS := tests

# Source files
SRC_FILES := $(wildcard $(addsuffix /*.c, $(SRC_DIRS)))
# Object files corresponding to source files
OBJ_FILES := $(SRC_FILES:.c=.o)

TEST_SRC_FILES := $(wildcard $(addsuffix /*.c, $(TEST_DIRS)))
TEST_OBJ_FILES := $(TEST_SRC_FILES:.c=.o)

# Unity and test files
SRC_UNITY = ./tests/Unity/src/unity.c
OBJ_UNITY = $(SRC_UNITY:.c=.o)
UNITY_INC_DIRS = -Isrc -I./tests/Unity/src

# Targets
TARGET_PING_CLIENT = ping_client
TARGET_MIP_DAEMON = mipd
TARGET_PING_SERVER = ping_server
TARGET_TEST_SERIALIZATION = serialize

# Specific object files
OBJ_PING_CLIENT = client/ping_client.o
OBJ_MIP_DAEMON = mip_daemon/mip_daemon.o
OBJ_PING_SERVER = server/ping_server.o
OBJ_COMMON = $(filter-out $(OBJ_PING_CLIENT) $(OBJ_MIP_DAEMON) $(OBJ_PING_SERVER), $(OBJ_FILES))

# Default target
all: $(TARGET_MIP_DAEMON) $(TARGET_PING_CLIENT) $(TARGET_PING_SERVER)


# Rule to build ping_client
$(TARGET_PING_CLIENT): $(OBJ_COMMON) $(OBJ_PING_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to build mipd
$(TARGET_MIP_DAEMON): $(OBJ_COMMON) $(OBJ_MIP_DAEMON)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to build ping_server
$(TARGET_PING_SERVER): $(OBJ_COMMON) $(OBJ_PING_SERVER)
	$(CC) $(CFLAGS) -o $@ $^

# Rule to build test serialization
test: $(OBJ_UNITY) $(TEST_OBJ_FILES) $(OBJ_COMMON)
	$(CC) $(UNITY_INC_DIRS) -o $(TARGET_TEST_SERIALIZATION) $^

# Rule to build all .o files from their corresponding .c files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove generated files
clean:
	rm -f $(OBJ_FILES) $(TARGET_MIP_DAEMON) $(TARGET_PING_CLIENT) $(TARGET_PING_SERVER) $(TARGET_TEST_SERIALIZATION) $(OBJ_UNITY) $(TEST_OBJ_FILES)

.PHONY: all clean test
