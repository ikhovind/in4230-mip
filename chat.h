/*
* File: chat.h
 *
 * Good practises for C header files.
 * 1. ALWAYS #include guards!
 * 2. #include only necessary libraries!
 * 3. DO NOT define global variables!
 *	  Are you sure you need them? There might be a better way.
 *	  If you still need them, make sure to employ the `extern` keyword.
 * 4. #define MACROs and declare functions prototype to be shared between
 *	  .c source files.
 *
 * Check https://valecs.gitlab.io/resources/CHeaderFileGuidelines.pdf for some
 * more nice practises.
 */

/*
 * #ifndef and #define are known as header guards.
 * Their primary purpose is to prevent header files
 * from being included multiple times.
 */

#ifndef _CHAT_H
#define _CHAT_H

/* MACROs */
#define MAX_CONNS 5   /* max. length of the pending connections queue */
#define MAX_EVENTS 10 /* max. number of concurrent events to check */

#include "./packet_builder/mip_builder.h"

/*
 * We declare the signature of a function in the header file
 * and its definition in the source file.
 *
 * return_type function_name(parameter1, parameter2, ...);
 */

void server(char* socket_name);
void client(char* socket_name);
void send_message(char* socket_name, mip_ping_sdu* mip_ping_sdu);

#endif /* _CHAT_H */
