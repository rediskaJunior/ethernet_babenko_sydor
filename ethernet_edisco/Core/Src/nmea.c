/*
 * nmea.c
 *
 *  Created on: Oct 4, 2025
 *      Author: Cerberus
 */


#include "nmea.h"
#include "string.h"
#include "stdio.h"
#include "ctype.h"
#include "gps.h"

#define NMEA_LINE_BUF 1024
static char linebuf[NMEA_LINE_BUF];
static size_t linebuf_pos = 0;

static nmea_stats_t stats;

void nmea_init(void) {
    memset(&stats, 0, sizeof(stats));
    linebuf_pos = 0;
}

/* ----- BEGIN: small compatibility / API shims for main.c ----- */
/* Add these symbols so existing main.c builds without changing main.c */

static volatile int new_pos_available = 0;
static gps_pos_t last_pos = {0};

/* Initialize parser (wrapper) */
void nmea_parser_init(void) {
    nmea_init();
    new_pos_available = 0;
    memset(&last_pos, 0, sizeof(last_pos));
}

/*
 * nmea_process()
 * Called by main loop to check if a new valid position arrived.
 * Returns 1 if new position is available (and clears the flag), 0 otherwise.
 */
int nmea_process(void) {
    if (new_pos_available) {
        new_pos_available = 0;
        return 1;
    }
    return 0;
}

/*
 * nmea_get_position()
 * Copies last parsed position into the provided structure.
 */
void nmea_get_position(gps_pos_t *out) {
    if (!out) return;
    *out = last_pos;
}


static unsigned char hex2int(char c) {
    if (c>='0' && c<='9') return c-'0';
    if (c>='A' && c<='F') return c-'A'+10;
    if (c>='a' && c<='f') return c-'a'+10;
    return 0;
}

static bool check_nmea_checksum(const char* s) {
    if (*s != '$') return false;
    const char* p = strchr(s, '*');
    if (!p) return false;
    unsigned char cs = 0;
    for (const char* q = s+1; q < p; ++q) cs ^= (unsigned char)*q;
    if (strlen(p) < 3) return false;
    unsigned char given = (hex2int(p[1]) << 4) | hex2int(p[2]);
    return cs == given;
}

static double parse_coord_ddmm_to_deg(const char* s) {
    if (!s || *s=='\0') return 0.0;
    double v = atof(s);
    double deg = floor(v / 100.0);
    double minutes = v - deg*100.0;
    return deg + minutes/60.0;
}

static void handle_nmea_line(const char* line) {
    stats.lines++;
    if (line[0] != '$') return;
    if (!check_nmea_checksum(line)) {
        stats.checksum_failed++;
        return;
    }
    stats.valid++;
    char tmp[256];
    strncpy(tmp, line, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1]=0;
    char* token = strtok(tmp, ",");
    if (!token) return;
    if (strcmp(token+3, "RMC")==0) {
        char* time_s = strtok(NULL, ",");
        char* status = strtok(NULL, ",");
        char* lat = strtok(NULL, ",");
        char* lat_ns = strtok(NULL, ",");
        char* lon = strtok(NULL, ",");
        char* lon_ew = strtok(NULL, ",");
        char* spd = strtok(NULL, ",");
        char* track = strtok(NULL, ",");
        char* date_s = strtok(NULL, ",");
        if (status && status[0]=='A' && lat && lon && date_s && time_s) {
            double latd = parse_coord_ddmm_to_deg(lat);
            if (lat_ns && lat_ns[0]=='S') latd = -latd;
            double lond = parse_coord_ddmm_to_deg(lon);
            if (lon_ew && lon_ew[0]=='W') lond = -lond;
            int hh=0,mm=0,ss=0, dd=0,mon=0,yy=0;
            if (strlen(time_s)>=6) {
                char buf[5];
                strncpy(buf, time_s, 2); buf[2]=0; hh=atoi(buf);
                strncpy(buf, time_s+2,2); buf[2]=0; mm=atoi(buf);
                strncpy(buf, time_s+4,2); buf[2]=0; ss=atoi(buf);
            }
            if (strlen(date_s)>=6) {
                char b[3];
                strncpy(b, date_s,2); b[2]=0; dd=atoi(b);
                strncpy(b, date_s+2,2); b[2]=0; mon=atoi(b);
                strncpy(b, date_s+4,2); b[2]=0; yy=atoi(b);
                yy += 2000;
            }
            gps_on_new_position(latd, lond, 1, 0, yy,mon,dd,hh,mm,ss);
        }
    } else if (strcmp(token+3, "GGA")==0) {
        char* time_s = strtok(NULL, ",");
        char* lat = strtok(NULL, ",");
        char* lat_ns = strtok(NULL, ",");
        char* lon = strtok(NULL, ",");
        char* lon_ew = strtok(NULL, ",");
        char* fix = strtok(NULL, ",");
        char* sats = strtok(NULL, ",");
        int fix_i = fix ? atoi(fix) : 0;
        int sats_i = sats ? atoi(sats) : 0;
        if (fix_i>0 && lat && lon) {
            double latd = parse_coord_ddmm_to_deg(lat);
            if (lat_ns && lat_ns[0]=='S') latd = -latd;
            double lond = parse_coord_ddmm_to_deg(lon);
            if (lon_ew && lon_ew[0]=='W') lond = -lond;
            int hh=0,mm=0,ss=0;
            if (strlen(time_s)>=6) {
                char buf[3];
                buf[2]=0;
                buf[0]=time_s[0];buf[1]=time_s[1]; hh=atoi(buf);
                buf[0]=time_s[2];buf[1]=time_s[3]; mm=atoi(buf);
                buf[0]=time_s[4];buf[1]=time_s[5]; ss=atoi(buf);
            }
            gps_on_new_position(latd, lond, (uint8_t)fix_i, (uint8_t)sats_i, 0,0,0,hh,mm,ss);
        }
    }
}

void nmea_push_chunk(const uint8_t *buf, size_t len) {
    for (size_t i=0;i<len;i++) {
        char c = (char)buf[i];
        if (linebuf_pos < (NMEA_LINE_BUF-1)) {
            linebuf[linebuf_pos++] = c;
            linebuf[linebuf_pos] = 0;
        } else {
            linebuf_pos = 0;
        }
        if (c == '\n') {
            char *start = strchr(linebuf, '$');
            if (start) {
                handle_nmea_line(start);
            }
            linebuf_pos = 0;
        }
    }
}

nmea_stats_t nmea_get_stats(void) {
    return stats;
}
