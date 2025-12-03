#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <stdint.h>
#include "w5500.h"

/* Max sockets = 8 */
#define MAX_SOCK_NUM 8

int socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flags);
int listen_socket(uint8_t sn);
int accept_socket(uint8_t sn);
int connect_socket(uint8_t sn, const uint8_t* addr, uint16_t port);
int send_socket(uint8_t sn, const uint8_t* buf, uint16_t len);
int recv_socket(uint8_t sn, uint8_t* buf, uint16_t len);
int disconnect_socket(uint8_t sn);
int close_socket(uint8_t sn);

#endif
