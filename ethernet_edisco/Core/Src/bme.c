/*
 * bme.c
 *
 *  Created on: Oct 7, 2025
 *      Author: Cerberus
 */


#include "bme.h"
#include "main.h"
#include <string.h>
#include <math.h>

#define BME280_ADDR  (0x76 << 1) // or 0x77 << 1 depending on SDO

#define REG_ID        0xD0
#define REG_RESET     0xE0
#define REG_CTRL_HUM  0xF2
#define REG_STATUS    0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG    0xF5
#define REG_PRESS_MSB 0xF7

static I2C_HandleTypeDef *bme_i2c;

static uint16_t dig_T1;
static int16_t dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;
static int32_t  t_fine;

static void bme_read_calibration(void);
static uint8_t bme_read8(uint8_t reg);
static void bme_read_buf(uint8_t reg, uint8_t *buf, uint16_t len);
static void bme_write8(uint8_t reg, uint8_t val);

void bme280_init(I2C_HandleTypeDef *hi2c)
{
    bme_i2c = hi2c;

    uint8_t id = bme_read8(REG_ID);
    if (id != 0x60) return;

    bme_write8(REG_RESET, 0xB6);
    HAL_Delay(10);

    bme_read_calibration();

    bme_write8(REG_CTRL_HUM, 0x01);
    bme_write8(REG_CONFIG, 0x00);
    bme_write8(REG_CTRL_MEAS, 0x25);
}

void bme280_forced_measure(void)
{
    uint8_t ctrl = 0x25 | 0x01; // forced mode
    bme_write8(REG_CTRL_MEAS, ctrl);
}

bool bme280_read(bme280_data_t *out)
{
    uint8_t buf[8];
    bme_read_buf(REG_PRESS_MSB, buf, 8);

    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    int32_t adc_H = ((int32_t)buf[6] << 8)  | buf[7];

    // temperature
    int32_t var1 = ((((adc_T>>3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T>>4) - ((int32_t)dig_T1)) * ((adc_T>>4) - ((int32_t)dig_T1))) >> 12) *
                    ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    float T = (t_fine * 5 + 128) >> 8;
    out->temperature = T / 100.0f;

    // pressure
    int64_t var1p = ((int64_t)t_fine) - 128000;
    int64_t var2p = var1p * var1p * (int64_t)dig_P6;
    var2p = var2p + ((var1p * (int64_t)dig_P5) << 17);
    var2p = var2p + (((int64_t)dig_P4) << 35);
    var1p = ((var1p * var1p * (int64_t)dig_P3) >> 8) + ((var1p * (int64_t)dig_P2) << 12);
    var1p = (((((int64_t)1) << 47) + var1p)) * ((int64_t)dig_P1) >> 33;
    if (var1p == 0) return false;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2p) * 3125) / var1p;
    var1p = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2p = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1p + var2p) >> 8) + (((int64_t)dig_P7) << 4);
    out->pressure = (float)p / 25600.0f;

    // humidity
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) -
                    (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                  (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
                       (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                     ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
    v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                               ((int32_t)dig_H1)) >> 4);
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    out->humidity = (v_x1_u32r >> 12) / 1024.0f;

    out->last_update = HAL_GetTick();
    out->valid = true;
    return true;
}

static void bme_read_calibration(void)
{
    uint8_t calib1[26];
    bme_read_buf(0x88, calib1, 26);

    dig_T1 = (uint16_t)(calib1[1] << 8 | calib1[0]);
    dig_T2 = (int16_t)(calib1[3] << 8 | calib1[2]);
    dig_T3 = (int16_t)(calib1[5] << 8 | calib1[4]);
    dig_P1 = (uint16_t)(calib1[7] << 8 | calib1[6]);
    dig_P2 = (int16_t)(calib1[9] << 8 | calib1[8]);
    dig_P3 = (int16_t)(calib1[11] << 8 | calib1[10]);
    dig_P4 = (int16_t)(calib1[13] << 8 | calib1[12]);
    dig_P5 = (int16_t)(calib1[15] << 8 | calib1[14]);
    dig_P6 = (int16_t)(calib1[17] << 8 | calib1[16]);
    dig_P7 = (int16_t)(calib1[19] << 8 | calib1[18]);
    dig_P8 = (int16_t)(calib1[21] << 8 | calib1[20]);
    dig_P9 = (int16_t)(calib1[23] << 8 | calib1[22]);
    dig_H1 = calib1[25];

    uint8_t calib2[7];
    bme_read_buf(0xE1, calib2, 7);
    dig_H2 = (int16_t)(calib2[1] << 8 | calib2[0]);
    dig_H3 = calib2[2];
    dig_H4 = (int16_t)((calib2[3] << 4) | (calib2[4] & 0x0F));
    dig_H5 = (int16_t)((calib2[5] << 4) | (calib2[4] >> 4));
    dig_H6 = (int8_t)calib2[6];
}

static uint8_t bme_read8(uint8_t reg)
{
    uint8_t val;
    HAL_I2C_Mem_Read(bme_i2c, BME280_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, HAL_MAX_DELAY);
    return val;
}

static void bme_read_buf(uint8_t reg, uint8_t *buf, uint16_t len)
{
    HAL_I2C_Mem_Read(bme_i2c, BME280_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, HAL_MAX_DELAY);
}

static void bme_write8(uint8_t reg, uint8_t val)
{
    HAL_I2C_Mem_Write(bme_i2c, BME280_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, HAL_MAX_DELAY);
}
