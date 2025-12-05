

#ifndef INC_BME_H_
#define INC_BME_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float temperature;   // °C
    float pressure;      // hPa
    float humidity;      // %RH
    uint32_t last_update;
    bool valid;
} bme280_data_t;

void bme280_init(I2C_HandleTypeDef *hi2c);
bool bme280_read(bme280_data_t *out);
void bme280_forced_measure(void);



#endif /* INC_BME_H_ */
