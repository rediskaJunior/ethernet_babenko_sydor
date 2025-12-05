#include "socket.h"
#include "w5500.h"
#include "main.h"
#include <stdio.h>
#include <ctype.h>


/**
 * Open a socket
 * @param sn Socket number (0-7)
 * @param protocol W5500_Sn_MR_TCP (0x01) or W5500_Sn_MR_UDP (0x02)
 * @param port Local port number
 * @param flags Additional flags (usually 0)
 * @return 0 on success, negative on error
 */
int socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flags)
{
    // Close socket if it's already open
    uint8_t status = W5500_READ_REG(W5500_Sn_SR(sn));
    if (status != W5500_SR_SOCK_CLOSED) {
        close_socket(sn);
    }

    // Set socket mode register (TCP/UDP)
    W5500_WRITE_REG(W5500_Sn_MR(sn), protocol | flags);

    // Set source port
    W5500_WRITE_REG(W5500_Sn_PORT0(sn), (port >> 8) & 0xFF);
    W5500_WRITE_REG(W5500_Sn_PORT0(sn) + 1, port & 0xFF);

    // Send OPEN command
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_OPEN);

    // Wait for command to complete (max 500ms)
    uint32_t timeout = HAL_GetTick() + 500;
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0) {
        if (HAL_GetTick() > timeout) {
            return -1;  // Timeout
        }
        HAL_Delay(1);
    }

    // Verify socket opened successfully
    status = W5500_READ_REG(W5500_Sn_SR(sn));
    if (status != W5500_SR_SOCK_INIT && status != W5500_SR_SOCK_UDP) {
        return -2;  // Socket not in expected state
    }

    return 0;
}

/**
 * Put TCP socket into LISTEN mode
 */
int listen_socket(uint8_t sn)
{
    // Check if socket is in INIT state
    uint8_t status = W5500_READ_REG(W5500_Sn_SR(sn));
    if (status != W5500_SR_SOCK_INIT) {
        return -1;  // Socket not initialized
    }

    // Send LISTEN command
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_LISTEN);

    // Wait for command to complete (max 500ms)
    uint32_t timeout = HAL_GetTick() + 500;
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0) {
        if (HAL_GetTick() > timeout) {
            return -2;  // Timeout
        }
        HAL_Delay(1);
    }

    // Verify socket is listening
    status = W5500_READ_REG(W5500_Sn_SR(sn));
    if (status != W5500_SR_SOCK_LISTEN) {
        return -3;  // Socket not listening
    }

    return 0;
}

/**
 * Accept connection (automatic in W5500)
 */
int accept_socket(uint8_t sn)
{
    return 0;
}

/**
 * Connect to remote host (TCP client)
 */
int connect_socket(uint8_t sn, const uint8_t* addr, uint16_t port)
{
    // Set destination IP
    for (int i = 0; i < 4; i++) {
        W5500_WRITE_REG(W5500_Sn_DIPR0(sn) + i, addr[i]);
    }

    // Set destination port
    W5500_WRITE_REG(W5500_Sn_DPORT0(sn), (port >> 8) & 0xFF);
    W5500_WRITE_REG(W5500_Sn_DPORT0(sn) + 1, port & 0xFF);

    // Send CONNECT command
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_CONNECT);

    // Wait for command to complete
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);

    return 0;
}

/**
 * Send data through socket
 */
int send_socket(uint8_t sn, const uint8_t* buf, uint16_t len)
{
    if (len == 0) return 0;

    // Get current TX write pointer
    uint16_t ptr = W5500_READ_REG16(W5500_Sn_TX_WR0(sn));

    // Calculate physical address in TX buffer
    // TX buffer base: 0x8000 + (socket_num * 0x0800)
    uint16_t offset = ptr & 0x07FF;  // Mask to 2KB buffer size
    uint16_t addr = 0x8000 + (sn * 0x0800) + offset;

    // Write data to TX buffer
    W5500_WRITE_BUF(addr, buf, len);

    // Update TX write pointer
    ptr += len;
    W5500_WRITE_REG(W5500_Sn_TX_WR0(sn), (ptr >> 8) & 0xFF);
    W5500_WRITE_REG(W5500_Sn_TX_WR0(sn) + 1, ptr & 0xFF);

    // Send SEND command
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_SEND);

    // Wait for SEND command to complete
    uint32_t timeout = HAL_GetTick() + 1000;
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0) {
        if (HAL_GetTick() > timeout) {
            return -1;  // Timeout
        }
    }

    return len;
}

/**
 * Receive data from socket
 */
int recv_socket(uint8_t sn, uint8_t* buf, uint16_t len)
{
    // Check received size
    uint16_t recv_size = W5500_READ_REG16(W5500_Sn_RX_RSR0(sn));
    if (recv_size == 0) return 0;

    // Limit to requested length
    if (recv_size > len) {
        recv_size = len;
    }

    // Get current RX read pointer
    uint16_t ptr = W5500_READ_REG16(W5500_Sn_RX_RD0(sn));

    // Calculate physical address in RX buffer
    // RX buffer base: 0xC000 + (socket_num * 0x0800)
    uint16_t offset = ptr & 0x07FF;  // Mask to 2KB buffer size
    uint16_t addr = 0xC000 + (sn * 0x0800) + offset;

    // Read data from RX buffer
    W5500_READ_BUF(addr, buf, recv_size);

    // Update RX read pointer
    ptr += recv_size;
    W5500_WRITE_REG(W5500_Sn_RX_RD0(sn), (ptr >> 8) & 0xFF);
    W5500_WRITE_REG(W5500_Sn_RX_RD0(sn) + 1, ptr & 0xFF);

    // Send RECV command to update RX buffer
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_RECV);

    // Wait for command to complete
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);

    return recv_size;
}

/**
 * Disconnect socket
 */
int disconnect_socket(uint8_t sn)
{
    // Send DISCON command
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_DISCON);

    // Wait for command to complete (max 1 second)
    uint32_t timeout = HAL_GetTick() + 1000;
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0) {
        if (HAL_GetTick() > timeout) {
            return -1;
        }
    }

    return 0;
}

/**
 * Close socket
 */
int close_socket(uint8_t sn)
{
    // Send CLOSE command
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_CLOSE);

    // Wait for command to complete (max 1 second)
    uint32_t timeout = HAL_GetTick() + 1000;
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0) {
        if (HAL_GetTick() > timeout) {
            return -1;
        }
    }

    // Wait until socket status is CLOSED
    timeout = HAL_GetTick() + 1000;
    while (W5500_READ_REG(W5500_Sn_SR(sn)) != W5500_SR_SOCK_CLOSED) {
        if (HAL_GetTick() > timeout) {
            return -2;
        }
        HAL_Delay(1);
    }

    // Clear interrupt flags
    W5500_WRITE_REG(W5500_Sn_IR(sn), 0xFF);

    return 0;
}

/**
 * Get socket status
 */
uint8_t get_socket_status(uint8_t sn)
{
    return W5500_READ_REG(W5500_Sn_SR(sn));
}


static int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) return diff;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
