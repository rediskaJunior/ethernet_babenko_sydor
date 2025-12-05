#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <stdint.h>

/**
 * Open a socket
 * @param sn Socket number (0-7)
 * @param protocol 0x01=TCP, 0x02=UDP
 * @param port Local port number
 * @param flags Additional flags (usually 0)
 * @return 0 on success, negative on error
 */
int socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flags);

/**
 * Put TCP socket into LISTEN mode (server)
 * @param sn Socket number
 * @return 0 on success, negative on error
 */
int listen_socket(uint8_t sn);

/**
 * Accept incoming connection (automatic in W5500)
 * @param sn Socket number
 * @return 0 (always succeeds)
 */
int accept_socket(uint8_t sn);

/**
 * Connect to remote host (TCP client)
 * @param sn Socket number
 * @param addr Remote IP address (4 bytes)
 * @param port Remote port number
 * @return 0 on success, negative on error
 */
int connect_socket(uint8_t sn, const uint8_t* addr, uint16_t port);

/**
 * Send data through socket
 * @param sn Socket number
 * @param buf Data buffer to send
 * @param len Length of data
 * @return Number of bytes sent, or negative on error
 */
int send_socket(uint8_t sn, const uint8_t* buf, uint16_t len);

/**
 * Receive data from socket
 * @param sn Socket number
 * @param buf Buffer to store received data
 * @param len Maximum bytes to receive
 * @return Number of bytes received, 0 if no data, negative on error
 */
int recv_socket(uint8_t sn, uint8_t* buf, uint16_t len);

/**
 * Disconnect socket (TCP)
 * @param sn Socket number
 * @return 0 on success, negative on error
 */
int disconnect_socket(uint8_t sn);

/**
 * Close socket
 * @param sn Socket number
 * @return 0 on success, negative on error
 */
int close_socket(uint8_t sn);

/**
 * Get socket status
 * @param sn Socket number
 * @return Socket status byte
 */
uint8_t get_socket_status(uint8_t sn);

#endif /* _SOCKET_H_ */
