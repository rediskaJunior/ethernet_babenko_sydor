#ifndef INC_GPS_H_
#define INC_GPS_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    double lat_deg;
    double lon_deg;
    uint8_t fix;
    uint8_t sats;
    uint16_t age_ms;
    bool valid;
    int year, month, day, hour, min, sec;
} gps_pos_t;

void gps_init(void);
gps_pos_t gps_get_last_position(void);
uint32_t gps_get_last_age_ms(void);
void format_lat_lon(double lat, double lon, char* out, int out_sz, int prec);
void format_utc_time(int year,int month,int day,int hour,int min,int sec, char* out, int out_sz);


#endif /* INC_GPS_H_ */
