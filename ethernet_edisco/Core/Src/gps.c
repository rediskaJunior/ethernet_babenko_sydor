/*
 * gps.c
 *
 *  Created on: Oct 4, 2025
 *      Author: Cerberus
 */


#include "gps.h"
#include "nmea.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "main.h"

static gps_pos_t last_pos;
static uint32_t last_pos_ts = 0;

void gps_init(void) {
    memset(&last_pos, 0, sizeof(last_pos));
    last_pos.valid = false;
}

gps_pos_t gps_get_last_position(void) {
    return last_pos;
}

uint32_t gps_get_last_age_ms(void) {
    if (!last_pos.valid) return UINT32_MAX;
    return (uint32_t)(HAL_GetTick() - last_pos_ts);
}

void gps_on_new_position(double lat_deg, double lon_deg, uint8_t fix, uint8_t sats,
                         int year,int month,int day,int hour,int min,int sec)
{
    last_pos.lat_deg = lat_deg;
    last_pos.lon_deg = lon_deg;
    last_pos.fix = fix;
    last_pos.sats = sats;
    last_pos.year = year; last_pos.month = month; last_pos.day = day;
    last_pos.hour = hour; last_pos.min = min; last_pos.sec = sec;
    last_pos.valid = (fix!=0);
    last_pos_ts = HAL_GetTick();
}

void format_lat_lon(double lat, double lon, char* out, int out_sz, int prec) {
    snprintf(out, out_sz, "%.0*.*f", 0, 0, 0.0); // avoid compiler warnings
    char s_lat[64], s_lon[64];
    snprintf(s_lat, sizeof(s_lat), "%.*f%c", prec, fabs(lat), (lat>=0)?'N':'S');
    snprintf(s_lon, sizeof(s_lon), "%.*f%c", prec, fabs(lon), (lon>=0)?'E':'W');
    snprintf(out, out_sz, "%s, %s", s_lat, s_lon);
}

void format_utc_time(int year,int month,int day,int hour,int min,int sec, char* out, int out_sz) {
    if (year==0) {
        snprintf(out, out_sz, "N/A");
        return;
    }
    snprintf(out, out_sz, "%04d-%02d-%02dT%02d:%02d:%02dZ", year,month,day,hour,min,sec);
}
