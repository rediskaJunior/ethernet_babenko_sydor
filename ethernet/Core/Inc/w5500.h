#ifndef _W5500_H_
#define _W5500_H_

#include <stdint.h>
#include "wizchip_conf.h"


/* Common registers */
#define W5500_MR         0x0000
#define W5500_GAR0       0x0001
#define W5500_SUBR0      0x0005
#define W5500_SHAR0      0x0009
#define W5500_SIPR0      0x000F

/* Socket base address */
#define W5500_Sn_MR(ch)          (0x4000 + (ch<<8))
#define W5500_Sn_CR(ch)          (0x4001 + (ch<<8))
#define W5500_Sn_IR(ch)          (0x4002 + (ch<<8))
#define W5500_Sn_SR(ch)          (0x4003 + (ch<<8))
#define W5500_Sn_PORT0(ch)       (0x4004 + (ch<<8))
#define W5500_Sn_TXMEM_SIZE(ch)  (0x401F + (ch<<8))
#define W5500_Sn_RXMEM_SIZE(ch)  (0x402F + (ch<<8))

/* Socket TX/RX buffer */
#define W5500_Sn_TX_WR0(ch)      (0x4024 + (ch<<8))
#define W5500_Sn_RX_RSR0(ch)     (0x4026 + (ch<<8))
#define W5500_Sn_RX_RD0(ch)      (0x4028 + (ch<<8))

/* Commands */
#define W5500_CR_OPEN     0x01
#define W5500_CR_LISTEN   0x02
#define W5500_CR_CONNECT  0x04
#define W5500_CR_DISCON   0x08
#define W5500_CR_CLOSE    0x10
#define W5500_CR_SEND     0x20
#define W5500_CR_RECV     0x40

/* Socket states */
#define W5500_SR_SOCK_CLOSED      0x00
#define W5500_SR_SOCK_INIT        0x13
#define W5500_SR_SOCK_LISTEN      0x14
#define W5500_SR_SOCK_ESTABLISHED 0x17

/* ==== Low level read/write ==== */
uint8_t  W5500_READ_REG(uint16_t addr);
void     W5500_WRITE_REG(uint16_t addr, uint8_t data);
uint16_t W5500_READ_REG16(uint16_t addr);
void     W5500_WRITE_BUF(uint16_t addr, const uint8_t* buf, uint16_t len);
void     W5500_READ_BUF(uint16_t addr, uint8_t* buf, uint16_t len);

#endif
