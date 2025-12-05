#include "wizchip_conf.h"
#include "w5500.h"
#include "main.h"
#include <string.h>

static wiz_NetInfo g_netinfo;

/**
 * Set network information to W5500
 */
void setnetinfo(const wiz_NetInfo* p)
{
    memcpy(&g_netinfo, p, sizeof(wiz_NetInfo));

    // Set Gateway Address
    for (int i = 0; i < 4; i++) {
        W5500_WRITE_REG(W5500_GAR0 + i, p->gw[i]);
    }

    // Set Subnet Mask
    for (int i = 0; i < 4; i++) {
        W5500_WRITE_REG(W5500_SUBR0 + i, p->sn[i]);
    }

    // Set MAC Address
    for (int i = 0; i < 6; i++) {
        W5500_WRITE_REG(W5500_SHAR0 + i, p->mac[i]);
    }

    // Set Source IP Address
    for (int i = 0; i < 4; i++) {
        W5500_WRITE_REG(W5500_SIPR0 + i, p->ip[i]);
    }
}

/**
 * Get current network information
 */
void wizchip_getnetinfo(wiz_NetInfo* p)
{
    if (p == NULL) return;

    // Read Gateway Address
    for (int i = 0; i < 4; i++) {
        p->gw[i] = W5500_READ_REG(W5500_GAR0 + i);
    }

    // Read Subnet Mask
    for (int i = 0; i < 4; i++) {
        p->sn[i] = W5500_READ_REG(W5500_SUBR0 + i);
    }

    // Read MAC Address
    for (int i = 0; i < 6; i++) {
        p->mac[i] = W5500_READ_REG(W5500_SHAR0 + i);
    }

    // Read Source IP Address
    for (int i = 0; i < 4; i++) {
        p->ip[i] = W5500_READ_REG(W5500_SIPR0 + i);
    }
}

/**
 * Initialize W5500 chip
 * @param tx_size Array of TX buffer sizes for 8 sockets (in KB: 0,1,2,4,8,16)
 * @param rx_size Array of RX buffer sizes for 8 sockets (in KB: 0,1,2,4,8,16)
 * @return 0 on success, -1 on failure
 */
int wizchip_init(const uint8_t* tx_size, const uint8_t* rx_size)
{
    // Hardware reset W5500
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(200);  // Wait for W5500 to boot up

    // Software reset
    W5500_WRITE_REG(W5500_MR, 0x80);
    HAL_Delay(10);

    // Wait for reset to complete
    uint32_t timeout = HAL_GetTick() + 1000;
    while (W5500_READ_REG(W5500_MR) & 0x80) {
        if (HAL_GetTick() > timeout) {
            return -1;  // Reset timeout
        }
        HAL_Delay(1);
    }

    // Verify communication - read version register
    uint8_t version = W5500_READ_REG(0x0039);  // VERSIONR
    // W5500 version should be 0x04
    if (version != 0x04) {
        // Communication may still work even if version doesn't match exactly
        // Don't fail here, just note it
    }

    // Configure socket buffer sizes
    // tx_size and rx_size values: 0,1,2,4,8,16 (in KB)
    // Register values: 0=1KB, 1=2KB, 2=4KB, 4=8KB, 8=16KB
    for (int i = 0; i < 8; i++) {
        W5500_WRITE_REG(W5500_Sn_TXBUF_SIZE(i), tx_size[i]);
        W5500_WRITE_REG(W5500_Sn_RXBUF_SIZE(i), rx_size[i]);
    }

    // Verify buffer configuration
    uint8_t tx_verify = W5500_READ_REG(W5500_Sn_TXBUF_SIZE(0));
    if (tx_verify != tx_size[0]) {
        return -2;  // Buffer configuration failed
    }

    return 0;
}
