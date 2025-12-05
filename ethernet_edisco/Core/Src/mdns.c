/*
 * mdns_minimal.c  - practical minimal mDNS responder for W5500 + STM32
 *
 * Behavior:
 *  - On mdns_init(hostname) opens UDP socket 5353 multicast and stores hostname.
 *  - Sends 3 multicast announcements on startup.
 *  - mdns_process() must be called frequently in main loop; it answers A queries
 *    for "<hostname>.local" by sending an mDNS A-record to multicast (224.0.0.251:5353).
 *
 * Notes:
 *  - We always reply via multicast. This is perfectly acceptable for mDNS and
 *    avoids differences in how some W5500 stacks populate peer registers.
 *  - Responses are simple, uncompressed names (safer for embedded).
 *  - Ensure mdns_init() is called after network is configured (after setnetinfo()/DHCP).
 */

#include "mdns.h"
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* configurable debug: enable to print debug lines (via UART/printf) */
#ifndef MDNS_DEBUG
#define MDNS_DEBUG 0
#endif

static char device_hostname[32] = "stm32-panel";
static uint8_t my_ip[4] = {0,0,0,0};
static const uint8_t mdns_mc_ip[4] = {224,0,0,251};
static const uint16_t mdns_port = 5353;
static const uint8_t MDNS_SOCKET_NUM = MDNS_SOCKET;

/* helper: case-insensitive compare */
static int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int diff = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (diff != 0) return diff;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/* Encode "label.label.local" into DNS wire-format into buf, returns bytes written.
   Writes full name (no compression). */
static uint16_t encode_dns_name(uint8_t* buf, const char* name) {
    uint16_t pos = 0;
    const char* start = name;
    const char* p = name;
    while (*p) {
        if (*p == '.') {
            uint8_t len = (uint8_t)(p - start);
            if (len == 0 || len >= 64) return 0; // invalid
            buf[pos++] = len;
            memcpy(&buf[pos], start, len);
            pos += len;
            start = p + 1;
        }
        p++;
    }
    uint8_t len = (uint8_t)(p - start);
    if (len > 0 && len < 64) {
        buf[pos++] = len;
        memcpy(&buf[pos], start, len);
        pos += len;
    }
    buf[pos++] = 0; /* null label terminator */
    return pos;
}

/* Build a single mDNS A-record response packet (multicast) into resp buffer.
   Returns packet length in bytes, or 0 on error.
   We use ID = 0 (per mDNS convention) and QR=1, AA set, ANCOUNT=1.
   Class set to 0x8001 (cache-flush + IN).
*/
static uint16_t build_mdns_a_response(uint8_t* resp, uint16_t resp_max, const char* fullname, const uint8_t ip4[4]) {
    if (!resp || !fullname || resp_max < 64) return 0;

    uint16_t pos = 0;

    /* Transaction ID: 0x0000 (mDNS convention) */
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;

    /* Flags: 0x8400 (QR=1 response, AA=1). This is common for authoritative mDNS replies. */
    resp[pos++] = 0x84;
    resp[pos++] = 0x00;

    /* QDCOUNT = 0 (we are sending only an answer), ANCOUNT = 1, NSCOUNT = 0, ARCOUNT = 0 */
    resp[pos++] = 0x00; resp[pos++] = 0x00; /* QDCOUNT */
    resp[pos++] = 0x00; resp[pos++] = 0x01; /* ANCOUNT = 1 */
    resp[pos++] = 0x00; resp[pos++] = 0x00; /* NSCOUNT */
    resp[pos++] = 0x00; resp[pos++] = 0x00; /* ARCOUNT */

    /* Answer section: NAME (fullname in wire format) */
    uint16_t name_len = encode_dns_name(&resp[pos], fullname);
    if (name_len == 0) return 0;
    pos += name_len;

    /* TYPE A (0x0001) */
    resp[pos++] = 0x00;
    resp[pos++] = 0x01;

    /* CLASS: 0x8001 -> cache-flush bit (0x8000) + IN(0x0001) */
    resp[pos++] = 0x80;
    resp[pos++] = 0x01;

    /* TTL: 120 seconds */
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 0x78;

    /* RDLENGTH = 4 */
    resp[pos++] = 0x00;
    resp[pos++] = 0x04;

    /* RDATA = IPv4 address */
    resp[pos++] = ip4[0];
    resp[pos++] = ip4[1];
    resp[pos++] = ip4[2];
    resp[pos++] = ip4[3];

    return pos;
}

/* Low-level send: program destination registers for socket and call send_socket.
   We always set destination to multicast 224.0.0.251:5353 for mDNS replies.
*/
static int mdns_send_multicast(uint8_t sn, const uint8_t* packet, uint16_t packet_len) {
    if (!packet || packet_len == 0) return -1;

    /* program destination IP/port and MAC to multicast */
    uint8_t mc_mac[6] = {0x01,0x00,0x5E,0x00,0x00,0xFB};
    uint8_t mc_ip[4] = {224,0,0,251};
    uint16_t mc_port = 5353;

    for (int i = 0; i < 6; ++i) {
        W5500_WRITE_REG(W5500_Sn_DHAR0(sn) + i, mc_mac[i]);
    }
    for (int i = 0; i < 4; ++i) {
        W5500_WRITE_REG(W5500_Sn_DIPR0(sn) + i, mc_ip[i]);
    }
    W5500_WRITE_REG(W5500_Sn_DPORT0(sn), (uint8_t)((mc_port >> 8) & 0xFF));
    W5500_WRITE_REG(W5500_Sn_DPORT0(sn) + 1, (uint8_t)(mc_port & 0xFF));

    /* send via socket API */
    int r = send_socket(sn, (uint8_t*)packet, packet_len);
#if MDNS_DEBUG
    printf("mdns: send_socket ret=%d len=%d\n", r, packet_len);
#endif
    return r;
}

/* Public: send one multicast announcement/response for our hostname */
void mdns_announce_once(void) {
    uint8_t sn = MDNS_SOCKET_NUM;

    /* refresh my_ip (in case DHCP changed it) */
    wiz_NetInfo net;
    wizchip_getnetinfo(&net);
    memcpy(my_ip, net.ip, 4);

    char fullname[64];
    snprintf(fullname, sizeof(fullname), "%s.local", device_hostname);

    uint8_t resp[256];
    uint16_t plen = build_mdns_a_response(resp, sizeof(resp), fullname, my_ip);
    if (plen == 0) return;

    mdns_send_multicast(sn, resp, plen);
}

/* Initialize mdns responder */
void mdns_init(const char* hostname) {
    if (hostname && hostname[0]) {
        int j = 0;
        for (int i = 0; hostname[i] && j < (int)(sizeof(device_hostname)-1); ++i) {
            if (hostname[i] != '.' && hostname[i] != ' ')
                device_hostname[j++] = hostname[i];
        }
        device_hostname[j] = '\0';
    }

    /* initial read IP */
    wiz_NetInfo net;
    wizchip_getnetinfo(&net);
    memcpy(my_ip, net.ip, 4);

    uint8_t sn = MDNS_SOCKET_NUM;
    close_socket(sn);
    HAL_Delay(5);

    /* Open socket: UDP with MULTI flag (some W5500 variants require Sn_MR_MULTI) */
    socket(sn, Sn_MR_UDP | Sn_MR_MULTI, mdns_port, 0);
    HAL_Delay(5);

    /* Program multicast MAC/IP in destination registers so the W5500 accepts mDNS */
    uint8_t mc_mac[6] = {0x01,0x00,0x5E,0x00,0x00,0xFB};
    uint8_t mc_ip[4] = {224,0,0,251};
    for (int i = 0; i < 6; ++i) W5500_WRITE_REG(W5500_Sn_DHAR0(sn) + i, mc_mac[i]);
    for (int i = 0; i < 4; ++i) W5500_WRITE_REG(W5500_Sn_DIPR0(sn) + i, mc_ip[i]);

    listen_socket(sn);

#if MDNS_DEBUG
    printf("mdns: init hostname='%s' ip=%d.%d.%d.%d socket=%d\n",
           device_hostname, my_ip[0], my_ip[1], my_ip[2], my_ip[3], sn);
#endif

    /* Send three announcements (standard mDNS practice) */
    HAL_Delay(50);
    mdns_announce_once();
    HAL_Delay(250);
    mdns_announce_once();
    HAL_Delay(250);
    mdns_announce_once();
}

/* Call periodically from main loop */
void mdns_process(void) {
    uint8_t sn = MDNS_SOCKET_NUM;
    uint8_t status = get_socket_status(sn);

    /* ensure socket is in a usable state */
    if (status == 0x00 || status == 0x1F) { /* CLOSED or UNKNOWN */
        close_socket(sn);
        HAL_Delay(5);
        socket(sn, Sn_MR_UDP | Sn_MR_MULTI, mdns_port, 0);
        HAL_Delay(5);
        listen_socket(sn);
        return;
    }

    /* Check RX size */
    uint16_t rx_size = W5500_READ_REG16(W5500_Sn_RX_RSR0(sn));
    if (rx_size == 0) return;

    if (rx_size > 512) rx_size = 512;
    uint8_t buf[512];
    int received = recv_socket(sn, buf, rx_size);
    if (received <= 12) return; /* too small for DNS header */

#if MDNS_DEBUG
    printf("mdns: got %d bytes\n", received);
    for (int i = 0; i < (received < 32 ? received : 32); ++i) printf("%02X ", buf[i]);
    printf("\n");
#endif

    /* DNS header fields */
    uint16_t flags = (buf[2] << 8) | buf[3];
    uint16_t qdcount = (buf[4] << 8) | buf[5];

    /* Only handle queries (QR=0) and qdcount>0 */
    if ((flags & 0x8000) != 0) return;
    if (qdcount == 0) return;

    /* parse first question name */
    uint16_t pos = 12;
    char qname[128] = {0};
    pos = parse_dns_name(buf, received, pos, qname, sizeof(qname));
    if (pos == 0 || pos + 4 > received) return;

    uint16_t qtype = (buf[pos] << 8) | buf[pos+1];
    uint16_t qclass = (buf[pos+2] << 8) | buf[pos+3];

#if MDNS_DEBUG
    printf("mdns: qname='%s' qtype=0x%04X qclass=0x%04X\n", qname, qtype, qclass);
#endif

    /* We only answer Type A (1) / Class IN (1) queries */
    if (qtype != 0x0001) return;

    /* Build "<hostname>.local" */
    char fullname[128];
    snprintf(fullname, sizeof(fullname), "%s.local", device_hostname);

    /* Compare requested name with our hostname (case-insensitive) */
    if (my_strcasecmp(qname, fullname) == 0 || my_strcasecmp(qname, device_hostname) == 0) {
        /* refresh my_ip */
        wiz_NetInfo net2;
        wizchip_getnetinfo(&net2);
        memcpy(my_ip, net2.ip, 4);

        /* build response and multicast it */
        uint8_t resp[256];
        uint16_t plen = build_mdns_a_response(resp, sizeof(resp), fullname, my_ip);
        if (plen > 0) {
            mdns_send_multicast(sn, resp, plen);
#if MDNS_DEBUG
            printf("mdns: replied for '%s' ip=%d.%d.%d.%d\n", fullname, my_ip[0], my_ip[1], my_ip[2], my_ip[3]);
#endif
        }
    }
}
