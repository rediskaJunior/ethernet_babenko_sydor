#include "w5500.h"

/* ==== SPI Frame Format ====
   | Control Byte0 | Control Byte1 | Address ... | Data ... |
   W5500 uses: [addr high] [addr low] [control] [data...]
   Control bit:
     [7:3] Block select
     [2]   R/W
     [1:0] OM (0=var length)
*/

#define W5500_BSB_COMMON   0x00
#define W5500_READ         0x00
#define W5500_WRITE        0x04

static void w5500_write(uint16_t addr, const uint8_t* buf, uint16_t len)
{
    uint8_t hdr[3];

    hdr[0] = (addr >> 8) & 0xFF;
    hdr[1] = addr & 0xFF;
    hdr[2] = W5500_WRITE;

    wizchip_select();
    for (int i=0;i<3;i++) wiz_spi_writebyte(hdr[i]);
    for (int i=0;i<len;i++) wiz_spi_writebyte(buf[i]);
    wizchip_deselect();
}

static void w5500_read(uint16_t addr, uint8_t* buf, uint16_t len)
{
    uint8_t hdr[3];

    hdr[0] = (addr >> 8) & 0xFF;
    hdr[1] = addr & 0xFF;
    hdr[2] = W5500_READ;

    wizchip_select();
    for (int i=0;i<3;i++) wiz_spi_writebyte(hdr[i]);
    for (int i=0;i<len;i++) buf[i] = wiz_spi_readbyte();
    wizchip_deselect();
}

/* ===== Public API ===== */

uint8_t W5500_READ_REG(uint16_t addr)
{
    uint8_t d;
    w5500_read(addr, &d, 1);
    return d;
}

void W5500_WRITE_REG(uint16_t addr, uint8_t data)
{
    w5500_write(addr, &data, 1);
}

uint16_t W5500_READ_REG16(uint16_t addr)
{
    uint8_t buf[2];
    w5500_read(addr, buf, 2);
    return (buf[0]<<8)|buf[1];
}

void W5500_WRITE_BUF(uint16_t addr, const uint8_t* buf, uint16_t len)
{
    w5500_write(addr, buf, len);
}

void W5500_READ_BUF(uint16_t addr, uint8_t* buf, uint16_t len)
{
    w5500_read(addr, buf, len);
}
