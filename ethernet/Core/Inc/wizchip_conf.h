#ifndef _WIZCHIP_CONF_H_
#define _WIZCHIP_CONF_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void wizchip_select(void);
void wizchip_deselect(void);
uint8_t wiz_spi_readbyte(void);
void wiz_spi_writebyte(uint8_t byte);

#define WIZCHIP_CRIS_ENTER()
#define WIZCHIP_CRIS_EXIT()

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t sn[4];
    uint8_t gw[4];
    uint8_t dns[4];
} wiz_NetInfo;

void setnetinfo(const wiz_NetInfo* p);
void wizchip_getnetinfo(wiz_NetInfo* p);

/* === Wizchip init === */
int wizchip_init(const uint8_t* tx_size, const uint8_t* rx_size);

#ifdef __cplusplus
}
#endif

#endif
