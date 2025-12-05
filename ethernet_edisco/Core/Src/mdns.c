/* mdns.c  - minimal mDNS responder for W5500 + STM32
 *
 * Usage:
 *   mdns_init("stm32f411panel");
 *   // call periodically:
 *   mdns_process();
 *
 * Notes:
 * - Adaptation for various W5500 driver variants: defines for Sn_MR_* provided here
 * - This responder answers A (type 1) queries for <hostname>.local with the device IP
 */

#include "mdns.h"
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifndef Sn_MR_TCP
#define Sn_MR_CLOSE   0x00
#define Sn_MR_TCP     0x01
#define Sn_MR_UDP     0x02
#define Sn_MR_MACRAW  0x04
#define Sn_MR_UCASTB  0x10
#define Sn_MR_ND      0x20
#define Sn_MR_MULTI   0x80
#endif

static char device_hostname[32] = "stm32-panel";
static uint8_t my_ip[4] = {0,0,0,0};

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

/* Encode "label.label.local" into DNS wire-format into buf, returns bytes written */
static uint16_t encode_dns_name(uint8_t* buf, const char* name) {
    uint16_t pos = 0;
    const char* start = name;
    const char* p = name;
    while (*p) {
        if (*p == '.') {
            uint8_t len = p - start;
            if (len > 0 && len < 64) {
                buf[pos++] = len;
                memcpy(&buf[pos], start, len);
                pos += len;
            }
            start = p + 1;
        }
        p++;
    }
    uint8_t len = p - start;
    if (len > 0 && len < 64) {
        buf[pos++] = len;
        memcpy(&buf[pos], start, len);
        pos += len;
    }
    buf[pos++] = 0;
    return pos;
}

/* parse DNS name (supports pointers) */
static uint16_t parse_dns_name(const uint8_t* packet, uint16_t packet_len,
                               uint16_t pos, char* name, uint16_t name_max) {
    uint16_t name_pos = 0;
    int jumps = 0;
    uint16_t return_pos = 0;

    while (pos < packet_len && jumps < 20) {
        uint8_t len = packet[pos++];
        if (len == 0) break;

        /* pointer */
        if ((len & 0xC0) == 0xC0) {
            if (pos >= packet_len) break;
            uint16_t off = ((len & 0x3F) << 8) | packet[pos++];
            if (return_pos == 0) return_pos = pos;
            pos = off;
            jumps++;
            continue;
        }

        if (pos + len > packet_len) break;
        if (name_pos != 0 && name_pos < name_max - 1) {
            name[name_pos++] = '.';
        }
        for (int i = 0; i < len && name_pos < name_max - 1; ++i) {
            name[name_pos++] = packet[pos++];
        }
    }
    name[name_pos] = '\0';
    return (return_pos != 0) ? return_pos : pos;
}

/* Compose and send A-record response. If dest_ip is multicast (224.0.0.251) it will be sent to multicast.
   dest_ip and dest_port are used to program Sn_DIPR0/Sn_DPORT0 before send_socket. */
static void send_dns_response(uint8_t sn, const uint8_t* dest_ip, uint16_t dest_port,
                              const uint8_t* query_packet, uint16_t query_len) {
    uint8_t resp[340];
    uint16_t pos = 0;

    /* Transaction ID: copy from query (first 2 bytes) - for mDNS typically 0x0000 but safe to copy */
    if (query_len >= 2) {
        resp[pos++] = query_packet[0];
        resp[pos++] = query_packet[1];
    } else {
        resp[pos++] = 0x00;
        resp[pos++] = 0x00;
    }

    /* Flags: Response. For mDNS we set 0x8400 (Authoritative) - but many queries use 0x0000; 0x8400 is OK */
    if (dest_port == 5353) {
        resp[pos++] = 0x84;
        resp[pos++] = 0x00;
    } else {
        resp[pos++] = 0x80;
        resp[pos++] = 0x00;
    }

    /* QDCOUNT = 0, ANCOUNT = 1, NSCOUNT = 0, ARCOUNT = 0 */
    resp[pos++] = 0x00; resp[pos++] = 0x00;
    resp[pos++] = 0x00; resp[pos++] = 0x01;
    resp[pos++] = 0x00; resp[pos++] = 0x00;
    resp[pos++] = 0x00; resp[pos++] = 0x00;

    /* Name: <hostname>.local */
    char fullname[64];
    if (dest_port == 5353) {
        snprintf(fullname, sizeof(fullname), "%s.local", device_hostname);
    } else {
        snprintf(fullname, sizeof(fullname), "%s", device_hostname);
    }
    pos += encode_dns_name(&resp[pos], fullname);

    /* Type A (0x0001) */
    resp[pos++] = 0x00;
    resp[pos++] = 0x01;

    /* Class IN (0x0001) with cache-flush (0x8001) for mDNS */
    if (dest_port == 5353) {
        resp[pos++] = 0x80;
        resp[pos++] = 0x01;
    } else {
        resp[pos++] = 0x00;
        resp[pos++] = 0x01;
    }

    /* TTL 120s */
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 0x00;
    resp[pos++] = 0x78;

    /* RDLENGTH = 4 */
    resp[pos++] = 0x00;
    resp[pos++] = 0x04;

    /* RDATA = my_ip */
    memcpy(&resp[pos], my_ip, 4);
    pos += 4;

    /* Program destination IP/port registers for the socket (Sn_DIPR0 / Sn_DPORT0) */
    for (int i = 0; i < 4; ++i) {
        W5500_WRITE_REG(W5500_Sn_DIPR0(sn) + i, dest_ip[i]);
    }
    W5500_WRITE_REG(W5500_Sn_DPORT0(sn), (uint8_t)((dest_port >> 8) & 0xFF));
    W5500_WRITE_REG(W5500_Sn_DPORT0(sn) + 1, (uint8_t)(dest_port & 0xFF));

    /* Send */
    send_socket(sn, resp, pos);
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

    /* read IP */
    wiz_NetInfo net;
    wizchip_getnetinfo(&net);
    memcpy(my_ip, net.ip, 4);

    /* ensure socket closed then open in UDP+MULTI mode bound to 5353 */
    uint8_t sn = MDNS_SOCKET;
    close_socket(sn);
    HAL_Delay(5);

    /* Open socket: UDP with MULTI bit */
    socket(sn, Sn_MR_UDP | Sn_MR_MULTI, 5353, 0);
    HAL_Delay(5);

    /* Set multicast destination registers so chip accepts multicast for this socket
       Multicast IPv4 mDNS address: 224.0.0.251
       Multicast MAC: 01:00:5E:00:00:FB
    */
    uint8_t mc_ip[4] = {224,0,0,251};
    uint8_t mc_mac[6] = {0x01,0x00,0x5E,0x00,0x00,0xFB};

    /* Some W5500 variants require writing Sn_DHAR0 (destination MAC) + Sn_DIPR0 before listen */
    for (int i = 0; i < 6; ++i) {
        W5500_WRITE_REG(W5500_Sn_DHAR0(sn) + i, mc_mac[i]);
    }
    for (int i = 0; i < 4; ++i) {
        W5500_WRITE_REG(W5500_Sn_DIPR0(sn) + i, mc_ip[i]);
    }

    /* Listen on socket (starts receiving) */
    listen_socket(sn);

    /* Optional: small delay then basic announcement could be sent from here if desired */
}

/* Call periodically from main loop */
void mdns_process(void) {
    uint8_t sn = MDNS_SOCKET;
    uint8_t status = get_socket_status(sn);

    /* If socket not ready, try reopen */
    if (status != W5500_SR_SOCK_UDP && status != W5500_SR_SOCK_ESTABLISHED && status != W5500_SR_SOCK_CLOSE_WAIT && status != W5500_SR_SOCK_INIT && status != W5500_SR_SOCK_LISTEN) {
        close_socket(sn);
        HAL_Delay(5);
        socket(sn, Sn_MR_UDP | Sn_MR_MULTI, 5353, 0);
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

    /* DNS header fields */
    uint16_t flags = (buf[2] << 8) | buf[3];
    uint16_t qdcount = (buf[4] << 8) | buf[5];

    /* Only handle queries (QR=0) and qdcount>0 */
    if ((flags & 0x8000) != 0) return;
    if (qdcount == 0) return;

    /* parse first question name */
    uint16_t pos = 12;
    char qname[128];
    pos = parse_dns_name(buf, received, pos, qname, sizeof(qname));
    if (pos + 4 > received) return;

    uint16_t qtype = (buf[pos] << 8) | buf[pos+1];
    uint16_t qclass = (buf[pos+2] << 8) | buf[pos+3];

    /* We only answer Type A (1) / Class IN (1) queries */
    if (qtype != 0x0001) return;

    /* Build local variants: "host.local" and "host" */
    char q_local[128];
    snprintf(q_local, sizeof(q_local), "%s.local", device_hostname);

    /* Compare requested name with our hostname */
    if (my_strcasecmp(qname, q_local) == 0 || my_strcasecmp(qname, device_hostname) == 0) {
        /* Try to get source IP/port from socket registers (works for many W5500 implementations)
           If not available, fallback to multicast address */
        uint8_t src_ip[4];
        uint8_t src_port_h = W5500_READ_REG(W5500_Sn_DIPR0(sn));
        (void)src_port_h; /* silence potential unused warning */

        /* W5500 stores peer IP/port in Sn_DIPR0/Sn_DPORT0 after recv; read them */
        for (int i = 0; i < 4; ++i) src_ip[i] = W5500_READ_REG(W5500_Sn_DIPR0(sn) + i);
        uint8_t src_port_hi = W5500_READ_REG(W5500_Sn_DPORT0(sn));
        uint8_t src_port_lo = W5500_READ_REG(W5500_Sn_DPORT0(sn) + 1);
        uint16_t src_port = ((uint16_t)src_port_hi << 8) | src_port_lo;

        /* decide destination: if src_ip is zero or equals our multicast, use multicast */
        uint8_t mc_ip[4] = {224,0,0,251};
        int src_is_valid = !(src_ip[0]==0 && src_ip[1]==0 && src_ip[2]==0 && src_ip[3]==0);

        if (src_is_valid) {
            /* respond unicast to src */
            send_dns_response(sn, src_ip, src_port, buf, received);
        } else {
            /* fallback: reply to multicast for mDNS */
            send_dns_response(sn, mc_ip, 5353, buf, received);
        }
    }

    /* After handling, no explicit disconnect needed for UDP; ready to receive more */
}
