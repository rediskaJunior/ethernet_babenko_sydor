
#ifndef INC_NMEA_H_
#define INC_NMEA_H_

#include <stdint.h>
#include <stddef.h>

void nmea_init(void);
void nmea_parser_init(void);


void nmea_push_chunk(const uint8_t *buf, size_t len);

typedef struct {
    uint32_t lines;
    uint32_t valid;
    uint32_t checksum_failed;
} nmea_stats_t;

nmea_stats_t nmea_get_stats(void);

int nmea_process(void);

#endif /* INC_NMEA_H_ */
