#ifndef _W5500_H_
#define _W5500_H_

#include <stdint.h>
#include "wizchip_conf.h"

/* ==== W5500 Register Address Map ==== */

/* Common Register Block (0x0000 - 0x003F) */
#define W5500_MR         0x0000  // Mode Register
#define W5500_GAR0       0x0001  // Gateway Address (4 bytes)
#define W5500_SUBR0      0x0005  // Subnet Mask (4 bytes)
#define W5500_SHAR0      0x0009  // Source Hardware Address (MAC, 6 bytes)
#define W5500_SIPR0      0x000F  // Source IP Address (4 bytes)
#define W5500_INTLEVEL0  0x0013  // Interrupt Low Level Timer
#define W5500_IR         0x0015  // Interrupt Register
#define W5500_IMR        0x0016  // Interrupt Mask Register
#define W5500_SIR        0x0017  // Socket Interrupt Register
#define W5500_SIMR       0x0018  // Socket Interrupt Mask Register
#define W5500_RTR0       0x0019  // Retry Time Register
#define W5500_RCR        0x001B  // Retry Count Register
#define W5500_PHYCFGR    0x002E  // PHY Configuration Register

/* Socket Register Blocks (0x4000 + socket_num * 0x100) */
#define W5500_Sn_MR(n)          (0x4000 + ((n) << 8) + 0x00)  // Socket n Mode Register
#define W5500_Sn_CR(n)          (0x4000 + ((n) << 8) + 0x01)  // Socket n Command Register
#define W5500_Sn_IR(n)          (0x4000 + ((n) << 8) + 0x02)  // Socket n Interrupt Register
#define W5500_Sn_SR(n)          (0x4000 + ((n) << 8) + 0x03)  // Socket n Status Register
#define W5500_Sn_PORT0(n)       (0x4000 + ((n) << 8) + 0x04)  // Socket n Source Port (2 bytes)
#define W5500_Sn_DHAR0(n)       (0x4000 + ((n) << 8) + 0x06)  // Socket n Dest HW Addr (6 bytes)
#define W5500_Sn_DIPR0(n)       (0x4000 + ((n) << 8) + 0x0C)  // Socket n Dest IP (4 bytes)
#define W5500_Sn_DPORT0(n)      (0x4000 + ((n) << 8) + 0x10)  // Socket n Dest Port (2 bytes)
#define W5500_Sn_MSSR0(n)       (0x4000 + ((n) << 8) + 0x12)  // Socket n Max Segment Size
#define W5500_Sn_TOS(n)         (0x4000 + ((n) << 8) + 0x15)  // Socket n IP TOS
#define W5500_Sn_TTL(n)         (0x4000 + ((n) << 8) + 0x16)  // Socket n IP TTL
#define W5500_Sn_RXBUF_SIZE(n)  (0x4000 + ((n) << 8) + 0x1E)  // Socket n RX Buffer Size
#define W5500_Sn_TXBUF_SIZE(n)  (0x4000 + ((n) << 8) + 0x1F)  // Socket n TX Buffer Size
#define W5500_Sn_TX_FSR0(n)     (0x4000 + ((n) << 8) + 0x20)  // Socket n TX Free Size (2 bytes)
#define W5500_Sn_TX_RD0(n)      (0x4000 + ((n) << 8) + 0x22)  // Socket n TX Read Pointer (2 bytes)
#define W5500_Sn_TX_WR0(n)      (0x4000 + ((n) << 8) + 0x24)  // Socket n TX Write Pointer (2 bytes)
#define W5500_Sn_RX_RSR0(n)     (0x4000 + ((n) << 8) + 0x26)  // Socket n RX Received Size (2 bytes)
#define W5500_Sn_RX_RD0(n)      (0x4000 + ((n) << 8) + 0x28)  // Socket n RX Read Pointer (2 bytes)
#define W5500_Sn_RX_WR0(n)      (0x4000 + ((n) << 8) + 0x2A)  // Socket n RX Write Pointer (2 bytes)
#define W5500_Sn_IMR(n)         (0x4000 + ((n) << 8) + 0x2C)  // Socket n Interrupt Mask
#define W5500_Sn_FRAG0(n)       (0x4000 + ((n) << 8) + 0x2D)  // Socket n Fragment (2 bytes)
#define W5500_Sn_KPALVTR(n)     (0x4000 + ((n) << 8) + 0x2F)  // Socket n Keep Alive Timer

// Legacy aliases for compatibility
#define W5500_Sn_TXMEM_SIZE(n)  W5500_Sn_TXBUF_SIZE(n)
#define W5500_Sn_RXMEM_SIZE(n)  W5500_Sn_RXBUF_SIZE(n)

/* Socket TX/RX Buffer Memory Map */
// TX Buffers: 0x8000 + (socket_num * 0x0800)  [2KB per socket]
// RX Buffers: 0xC000 + (socket_num * 0x0800)  [2KB per socket]

/* ==== Socket Commands (Sn_CR) ==== */
#define W5500_CR_OPEN       0x01
#define W5500_CR_LISTEN     0x02
#define W5500_CR_CONNECT    0x04
#define W5500_CR_DISCON     0x08
#define W5500_CR_CLOSE      0x10
#define W5500_CR_SEND       0x20
#define W5500_CR_SEND_MAC   0x21
#define W5500_CR_SEND_KEEP  0x22
#define W5500_CR_RECV       0x40

/* ==== Socket Status Values (Sn_SR) ==== */
#define W5500_SR_SOCK_CLOSED        0x00
#define W5500_SR_SOCK_INIT          0x13
#define W5500_SR_SOCK_LISTEN        0x14
#define W5500_SR_SOCK_SYNSENT       0x15
#define W5500_SR_SOCK_SYNRECV       0x16
#define W5500_SR_SOCK_ESTABLISHED   0x17
#define W5500_SR_SOCK_FIN_WAIT      0x18
#define W5500_SR_SOCK_CLOSING       0x1A
#define W5500_SR_SOCK_TIME_WAIT     0x1B
#define W5500_SR_SOCK_CLOSE_WAIT    0x1C
#define W5500_SR_SOCK_LAST_ACK      0x1D
#define W5500_SR_SOCK_UDP           0x22
#define W5500_SR_SOCK_IPRAW         0x32
#define W5500_SR_SOCK_MACRAW        0x42

/* ==== Socket Mode Values (Sn_MR) ==== */
#define W5500_Sn_MR_CLOSE       0x00
#define W5500_Sn_MR_TCP         0x01
#define W5500_Sn_MR_UDP         0x02
#define W5500_Sn_MR_IPRAW       0x03
#define W5500_Sn_MR_MACRAW      0x04
#define W5500_Sn_MR_PPPOE       0x05
#define W5500_Sn_MR_ND          0x20
#define W5500_Sn_MR_MC          0x20
#define W5500_Sn_MR_MFEN        0x80

/* ==== Socket Interrupt Flags (Sn_IR) ==== */
#define W5500_Sn_IR_SENDOK      0x10
#define W5500_Sn_IR_TIMEOUT     0x08
#define W5500_Sn_IR_RECV        0x04
#define W5500_Sn_IR_DISCON      0x02
#define W5500_Sn_IR_CON         0x01

/* ==== Low Level API ==== */
uint8_t  W5500_READ_REG(uint16_t addr);
void     W5500_WRITE_REG(uint16_t addr, uint8_t data);
uint16_t W5500_READ_REG16(uint16_t addr);
void     W5500_WRITE_BUF(uint16_t addr, const uint8_t* buf, uint16_t len);
void     W5500_READ_BUF(uint16_t addr, uint8_t* buf, uint16_t len);

#endif /* _W5500_H_ */
