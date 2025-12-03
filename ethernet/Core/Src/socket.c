#include "socket.h"

int socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flags)
{
    W5500_WRITE_REG(W5500_Sn_MR(sn), protocol);
    W5500_WRITE_REG(W5500_Sn_PORT0(sn), port >> 8);
    W5500_WRITE_REG(W5500_Sn_PORT0(sn)+1, port & 0xFF);

    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_OPEN);

    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0)
        ;

    return 0;
}

int listen_socket(uint8_t sn)
{
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_LISTEN);
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);
    return 0;
}

int accept_socket(uint8_t sn)
{
    return 0;
}

int connect_socket(uint8_t sn, const uint8_t* addr, uint16_t port)
{
    for(int i=0;i<4;i++)
        W5500_WRITE_REG(W5500_Sn_MR(sn)+0x0C+i, addr[i]);

    W5500_WRITE_REG(W5500_Sn_MR(sn)+0x10, port>>8);
    W5500_WRITE_REG(W5500_Sn_MR(sn)+0x11, port&0xFF);

    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_CONNECT);
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);
    return 0;
}

int send_socket(uint8_t sn, const uint8_t* buf, uint16_t len)
{
    uint16_t ptr = W5500_READ_REG16(W5500_Sn_TX_WR0(sn));
    uint16_t offset = ptr & 0x7FF;
    uint16_t addr = (sn<<8) | offset;

    W5500_WRITE_BUF(addr, buf, len);

    ptr += len;
    W5500_WRITE_REG(W5500_Sn_TX_WR0(sn), ptr>>8);
    W5500_WRITE_REG(W5500_Sn_TX_WR0(sn)+1, ptr&0xFF);

    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_SEND);
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);

    return len;
}

int recv_socket(uint8_t sn, uint8_t* buf, uint16_t len)
{
    uint16_t recv_size = W5500_READ_REG16(W5500_Sn_RX_RSR0(sn));
    if (recv_size == 0) return 0;

    if (recv_size < len) len = recv_size;

    uint16_t ptr = W5500_READ_REG16(W5500_Sn_RX_RD0(sn));
    uint16_t offset = ptr & 0x7FF;
    uint16_t addr = (sn<<8) | offset;

    W5500_READ_BUF(addr, buf, len);

    ptr += len;
    W5500_WRITE_REG(W5500_Sn_RX_RD0(sn), ptr>>8);
    W5500_WRITE_REG(W5500_Sn_RX_RD0(sn)+1, ptr&0xFF);

    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_RECV);
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);

    return len;
}

int disconnect_socket(uint8_t sn)
{
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_DISCON);
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);
    return 0;
}

int close_socket(uint8_t sn)
{
    W5500_WRITE_REG(W5500_Sn_CR(sn), W5500_CR_CLOSE);
    while (W5500_READ_REG(W5500_Sn_CR(sn)) != 0);
    return 0;
}
