#include "w5500.h"

/* ==== W5500 SPI Frame Format ====
   [Address High] [Address Low] [Control Phase] [Data...]

   Control Phase = [BSB(5-bit)][RWB(1-bit)][OM(2-bit)]
   - BSB: Block Select Bits
   - RWB: Read(0)/Write(1)
   - OM: Operation Mode (00 = VDM, Variable Data Length Mode)
*/

#define W5500_READ         0x00
#define W5500_WRITE        0x04

// Block Select Bit values
#define BSB_COMMON_REG     0x00
#define BSB_S0_REG         0x01
#define BSB_S0_TX_BUF      0x02
#define BSB_S0_RX_BUF      0x03
#define BSB_S1_REG         0x05
#define BSB_S1_TX_BUF      0x06
#define BSB_S1_RX_BUF      0x07
// ... and so on for sockets 2-7

/**
 * Get Block Select Bits based on address
 */
static uint8_t get_bsb(uint16_t addr)
{
    if (addr < 0x0040) {
        // Common register block (0x0000-0x003F)
        return BSB_COMMON_REG;
    }
    else if (addr >= 0x4000 && addr < 0x4800) {
        // Socket register blocks
        uint8_t sn = (addr - 0x4000) >> 8;  // Socket number (0-7)
        return (BSB_S0_REG + (sn * 4));
    }
    else if (addr >= 0x8000 && addr < 0xC000) {
        // TX buffer blocks
        uint8_t sn = (addr - 0x8000) / 0x0800;
        return (BSB_S0_TX_BUF + (sn * 4));
    }
    else if (addr >= 0xC000) {
        // RX buffer blocks
        uint8_t sn = (addr - 0xC000) / 0x0800;
        return (BSB_S0_RX_BUF + (sn * 4));
    }

    return BSB_COMMON_REG;
}

/**
 * Get actual address offset within block
 */
static uint16_t get_addr_offset(uint16_t addr)
{
    if (addr < 0x0040) {
        return addr;  // Common registers
    }
    else if (addr >= 0x4000 && addr < 0x4800) {
        return addr & 0x00FF;  // Socket registers
    }
    else if (addr >= 0x8000 && addr < 0xC000) {
        return addr & 0x07FF;  // TX buffer (2KB per socket)
    }
    else if (addr >= 0xC000) {
        return addr & 0x07FF;  // RX buffer (2KB per socket)
    }

    return addr;
}

/**
 * Low-level W5500 write
 */
static void w5500_write(uint16_t addr, const uint8_t* buf, uint16_t len)
{
    uint8_t bsb = get_bsb(addr);
    uint16_t offset = get_addr_offset(addr);

    uint8_t control = (bsb << 3) | W5500_WRITE;

    wizchip_select();

    // Send address (16-bit, MSB first)
    wiz_spi_writebyte((offset >> 8) & 0xFF);
    wiz_spi_writebyte(offset & 0xFF);

    // Send control byte
    wiz_spi_writebyte(control);

    // Send data
    for (uint16_t i = 0; i < len; i++) {
        wiz_spi_writebyte(buf[i]);
    }

    wizchip_deselect();
}

/**
 * Low-level W5500 read
 */
static void w5500_read(uint16_t addr, uint8_t* buf, uint16_t len)
{
    uint8_t bsb = get_bsb(addr);
    uint16_t offset = get_addr_offset(addr);

    uint8_t control = (bsb << 3) | W5500_READ;

    wizchip_select();

    // Send address (16-bit, MSB first)
    wiz_spi_writebyte((offset >> 8) & 0xFF);
    wiz_spi_writebyte(offset & 0xFF);

    // Send control byte
    wiz_spi_writebyte(control);

    // Read data
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = wiz_spi_readbyte();
    }

    wizchip_deselect();
}

/* ===== Public API ===== */

uint8_t W5500_READ_REG(uint16_t addr)
{
    uint8_t data;
    w5500_read(addr, &data, 1);
    return data;
}

void W5500_WRITE_REG(uint16_t addr, uint8_t data)
{
    w5500_write(addr, &data, 1);
}

uint16_t W5500_READ_REG16(uint16_t addr)
{
    uint8_t buf[2];
    w5500_read(addr, buf, 2);
    return (buf[0] << 8) | buf[1];
}

void W5500_WRITE_BUF(uint16_t addr, const uint8_t* buf, uint16_t len)
{
    w5500_write(addr, buf, len);
}

void W5500_READ_BUF(uint16_t addr, uint8_t* buf, uint16_t len)
{
    w5500_read(addr, buf, len);
}
