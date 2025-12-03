#include "wizchip_conf.h"
#include "w5500.h"
#include <string.h>

static wiz_NetInfo g_netinfo;

void setnetinfo(const wiz_NetInfo* p)
{
    memcpy(&g_netinfo, p, sizeof(wiz_NetInfo));

    /* MAC */
    for(int i=0;i<6;i++)
        W5500_WRITE_REG(W5500_SHAR0 + i, p->mac[i]);

    /* IP */
    for(int i=0;i<4;i++)
        W5500_WRITE_REG(W5500_SIPR0 + i, p->ip[i]);

    /* Subnet */
    for(int i=0;i<4;i++)
        W5500_WRITE_REG(W5500_SUBR0 + i, p->sn[i]);

    /* Gateway */
    for(int i=0;i<4;i++)
        W5500_WRITE_REG(W5500_GAR0 + i, p->gw[i]);

    /* DNS stored locally only */
}

void wizchip_getnetinfo(wiz_NetInfo* p)
{
    memcpy(p, &g_netinfo, sizeof(wiz_NetInfo));
}

int wizchip_init(const uint8_t* tx_size, const uint8_t* rx_size)
{
    W5500_WRITE_REG(W5500_MR, 0x80);
    for(volatile int i=0; i<100000; i++);

    for (int i=0; i<8; i++)
        W5500_WRITE_REG(W5500_Sn_TXMEM_SIZE(i), tx_size[i]);

    for (int i=0; i<8; i++)
        W5500_WRITE_REG(W5500_Sn_RXMEM_SIZE(i), rx_size[i]);

    return 0;
}
