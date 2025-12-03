/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "spi.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"
#include "socket.h"
#include "w5500.h"
#include "wizchip_conf.h"
//#include "dhcp.h"
#include "gps.h"
#include "nmea.h"
#include "bme.h"
#include "display_ili9341.h"
#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HTTP_SOCKET     0
#define DHCP_SOCKET     1
#define HTTP_PORT       80
#define DATA_BUF_SIZE   2048

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
wiz_NetInfo gWIZNETINFO = {
    .mac = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef},
    .ip = {192, 168, 1, 177},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 1, 1},
    .dns = {8, 8, 8, 8}
};

uint8_t rx_tx_buf[DATA_BUF_SIZE];
static uint8_t gps_rx_byte;

bme280_data_t bme_data = {0};
gps_pos_t gps_data = {0};

uint8_t net_initialized = 0;

static uint32_t gps_last_update = 0;
static uint32_t env_last_update = 0;

const char index_html[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>STM32 Network Panel</title>"
"<style>"
"body{font-family:Arial,sans-serif;background:#f0f0f0;margin:0;padding:20px;}"
"h1{text-align:center;color:#333;}"
".container{max-width:800px;margin:0 auto;}"
".card{background:white;border-radius:8px;padding:20px;margin:15px 0;box-shadow:0 2px 8px rgba(0,0,0,0.1);}"
".card h2{margin-top:0;color:#007BFF;border-bottom:2px solid #007BFF;padding-bottom:10px;}"
".row{display:flex;justify-content:space-between;margin:10px 0;}"
".label{font-weight:bold;color:#555;}"
".value{font-size:1.3em;color:#007BFF;font-weight:bold;}"
".stale{color:#ff4444;font-size:0.85em;font-weight:bold;}"
".good{color:#00aa00;}"
".badge{display:inline-block;padding:3px 8px;border-radius:4px;font-size:0.8em;font-weight:bold;}"
".fix-ok{background:#00aa00;color:white;}"
".fix-no{background:#ff4444;color:white;}"
"#timestamp{text-align:center;color:#888;font-size:0.9em;margin-top:10px;}"
"</style></head><body>"
"<div class='container'>"
"<h1>🌐 STM32 Device Status</h1>"
"<div class='card'>"
"<h2>📍 GPS Position</h2>"
"<div class='row'><span class='label'>Latitude:</span><span id='lat' class='value'>--</span></div>"
"<div class='row'><span class='label'>Longitude:</span><span id='lon' class='value'>--</span></div>"
"<div class='row'><span class='label'>Fix Status:</span><span id='fix' class='badge'>--</span></div>"
"<div class='row'><span class='label'>Satellites:</span><span id='sats' class='value'>--</span></div>"
"<div class='row'><span class='label'>UTC Time:</span><span id='utc' class='value'>--</span></div>"
"<div id='gps_stale' class='stale'></div>"
"</div>"
"<div class='card'>"
"<h2>🌡️ Environment Sensors</h2>"
"<div class='row'><span class='label'>Temperature:</span><span id='temp' class='value'>--</span> °C</div>"
"<div class='row'><span class='label'>Pressure:</span><span id='press' class='value'>--</span> hPa</div>"
"<div class='row'><span class='label'>Humidity:</span><span id='hum' class='value'>--</span> %</div>"
"<div id='env_stale' class='stale'></div>"
"</div>"
"<div id='timestamp'></div>"
"</div>"
"<script>"
"function upd(){"
"fetch('/status').then(r=>r.json()).then(d=>{"
"document.getElementById('lat').textContent=d.gps.lat.toFixed(5);"
"document.getElementById('lon').textContent=d.gps.lon.toFixed(5);"
"document.getElementById('sats').textContent=d.gps.sats;"
"document.getElementById('utc').textContent=d.time_utc;"
"let fixEl=document.getElementById('fix');"
"if(d.gps.fix>=2){fixEl.textContent='LOCKED';fixEl.className='badge fix-ok';}"
"else{fixEl.textContent='NO FIX';fixEl.className='badge fix-no';}"
"document.getElementById('temp').textContent=d.env.t_c.toFixed(1);"
"document.getElementById('press').textContent=d.env.p_hpa.toFixed(1);"
"document.getElementById('hum').textContent=d.env.rh_pct.toFixed(1);"
"document.getElementById('gps_stale').textContent=d.stale_age_s.gps>3?'⚠️ GPS data is stale ('+d.stale_age_s.gps.toFixed(1)+'s old)':'';"
"document.getElementById('env_stale').textContent=d.stale_age_s.env>3?'⚠️ Sensor data is stale ('+d.stale_age_s.env.toFixed(1)+'s old)':'';"
"document.getElementById('timestamp').textContent='Last updated: '+new Date().toLocaleTimeString();"
"}).catch(e=>{console.error(e);document.getElementById('timestamp').textContent='❌ Connection error';});}"
"setInterval(upd,1000);upd();"
"</script></body></html>";


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
#ifndef MX_USART1_UART_Init
void MX_USART1_UART_Init(void);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void wizchip_select(void) {
    HAL_GPIO_WritePin(W5550_CS_GPIO_Port, W5550_CS_Pin, GPIO_PIN_RESET);
}

void wizchip_deselect(void) {
    HAL_GPIO_WritePin(W5550_CS_GPIO_Port, W5550_CS_Pin, GPIO_PIN_SET);
}

uint8_t wiz_spi_readbyte(void) {
    uint8_t byte;
    HAL_SPI_Receive(&hspi2, &byte, 1, HAL_MAX_DELAY);
    return byte;
}

void wiz_spi_writebyte(uint8_t byte) {
    HAL_SPI_Transmit(&hspi2, &byte, 1, HAL_MAX_DELAY);
}

void dhcp_assign_callback(void) {
	wiz_NetInfo dhcpInfo;
	wizchip_getnetinfo(&dhcpInfo);

	memcpy(gWIZNETINFO.ip,  dhcpInfo.ip,  4);
	memcpy(gWIZNETINFO.sn,  dhcpInfo.sn,  4);
	memcpy(gWIZNETINFO.gw,  dhcpInfo.gw,  4);
	memcpy(gWIZNETINFO.dns, dhcpInfo.dns, 4);

    setnetinfo(&gWIZNETINFO);
    net_initialized = 1;
}
void network_init(void) {
    HAL_GPIO_WritePin(W5550_RST_GPIO_Port, W5550_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(W5550_RST_GPIO_Port, W5550_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(100);

    uint8_t memsize[8] = {2,2,2,2,2,2,2,2};
    if(wizchip_init(memsize, memsize) == -1) {
        ili9341_draw_text(10, 20, "W5500 Init FAIL!", &font6x8, 0xF800, 0x0000);
        while(1);
    }

    setnetinfo(&gWIZNETINFO);
    net_initialized = 1;
}

uint8_t get_socket_status(uint8_t sn) {
    return W5500_READ_REG(W5500_Sn_SR(sn));
}

void http_server_process(void) {
    uint8_t sn = HTTP_SOCKET;
    uint16_t size = 0;
    uint8_t status = get_socket_status(sn);

    switch(status) {
        case W5500_SR_SOCK_ESTABLISHED:
            size = W5500_READ_REG16(W5500_Sn_RX_RSR0(sn));

            if(size > 0) {
                if(size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
                recv_socket(sn, rx_tx_buf, size);
                rx_tx_buf[size] = '\0';

                if (strstr((char*)rx_tx_buf, "GET /status")) {
                    char json_buf[600];
                    uint32_t now = HAL_GetTick();

                    float gps_age = (float)(now - gps_last_update) / 1000.0f;
                    float env_age = (float)(now - env_last_update) / 1000.0f;
                    if(gps_last_update == 0) gps_age = 999.9f;
                    if(env_last_update == 0) env_age = 999.9f;

                    char time_str[32];
                    format_utc_time(gps_data.year, gps_data.month, gps_data.day,
                                   gps_data.hour, gps_data.min, gps_data.sec,
                                   time_str, sizeof(time_str));

                    printf(json_buf, sizeof(json_buf),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n"
                        "Cache-Control: no-cache\r\n\r\n"
                        "{"
                        "\"proto_ver\":1,"
                        "\"device_id\":\"bp-411-0007\","
                        "\"time_utc\":\"%s\","
                        "\"gps\":{\"lat\":%.6f,\"lon\":%.6f,\"fix\":%d,\"sats\":%d},"
                        "\"env\":{\"t_c\":%.1f,\"p_hpa\":%.1f,\"rh_pct\":%.1f,\"lux\":0},"
                        "\"stale_age_s\":{\"gps\":%.1f,\"env\":%.1f}"
                        "}",
                        time_str,
                        gps_data.lat_deg, gps_data.lon_deg, gps_data.fix, gps_data.sats,
                        bme_data.temperature, bme_data.pressure, bme_data.humidity,
                        gps_age, env_age
                    );
                    send_socket(sn, (uint8_t*)json_buf, strlen(json_buf));
                }
                else if (strstr((char*)rx_tx_buf, "GET / ")) {
                    char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
                    send_socket(sn, (uint8_t*)header, strlen(header));
                    send_socket(sn, (uint8_t*)index_html, strlen(index_html));
                }
                else {
                    char msg[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n404 Not Found";
                    send_socket(sn, (uint8_t*)msg, strlen(msg));
                }

                disconnect_socket(sn);
            }
            break;

        case W5500_SR_SOCK_LISTEN:
            break;

        case W5500_SR_SOCK_INIT:
            listen_socket(sn);
            break;

        case W5500_SR_SOCK_CLOSED:
            socket(sn, 0x01, HTTP_PORT, 0); // 0x01 = TCP
            break;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart->Instance == USART1) {
        nmea_push_chunk(&gps_rx_byte, 1);
        HAL_UART_Receive_IT(&huart1, &gps_rx_byte, 1);
    }
}

void display_update(void) {
    char buf[50];

    // Network info
    printf(buf, "IP: %d.%d.%d.%d",
            gWIZNETINFO.ip[0], gWIZNETINFO.ip[1],
            gWIZNETINFO.ip[2], gWIZNETINFO.ip[3]);
    ili9341_draw_text(10, 10, buf, &font6x8, 0x07E0, 0x0000);

    printf(buf, "GW: %d.%d.%d.%d",
            gWIZNETINFO.gw[0], gWIZNETINFO.gw[1],
            gWIZNETINFO.gw[2], gWIZNETINFO.gw[3]);
    ili9341_draw_text(10, 20, buf, &font6x8, 0xFFFF, 0x0000);

    // Sensor data
    printf(buf, "T: %.1fC  H: %.1f%%  ", bme_data.temperature, bme_data.humidity);
    ili9341_draw_text(10, 50, buf, &font6x8, 0xFFFF, 0x0000);

    printf(buf, "P: %.1f hPa        ", bme_data.pressure);
    ili9341_draw_text(10, 60, buf, &font6x8, 0xFFFF, 0x0000);

    // GPS data
    printf(buf, "Lat: %.5f   ", gps_data.lat_deg);
    ili9341_draw_text(10, 90, buf, &font6x8, 0xFFFF, 0x0000);

    printf(buf, "Lon: %.5f   ", gps_data.lon_deg);
    ili9341_draw_text(10, 100, buf, &font6x8, 0xFFFF, 0x0000);

    printf(buf, "Sats: %d  Fix: %d  ", gps_data.sats, gps_data.fix);
    ili9341_draw_text(10, 110, buf, &font6x8,
                     gps_data.fix >= 2 ? 0x07E0 : 0xF800, 0x0000);
}

/* USER CODE END 0 */

int main(void) {
	  HAL_Init();
	    SystemClock_Config();

	    MX_GPIO_Init();
	    MX_I2C1_Init();
	    MX_SPI1_Init();
	    MX_SPI2_Init();
	    MX_USART1_UART_Init();

	    ili9341_init(&hspi1);
	    ili9341_fill_screen(0x0000);
	    ili9341_draw_text(10, 150, "Initializing...", &font6x8, 0xFFFF, 0x0000);

	    bme280_init(&hi2c1);

	    gps_init();
	    nmea_init();

	    network_init();

	    HAL_UART_Receive_IT(&huart1, &gps_rx_byte, 1);

	    cli_init();

	    uint32_t last_sensor_poll = 0;
	    uint32_t last_display_update = 0;

	    while (1) {
	        uint32_t now = HAL_GetTick();

	        if(now - last_sensor_poll > 1000) {
	            last_sensor_poll = now;

	            bme280_forced_measure();
	            HAL_Delay(10);
	            if(bme280_read(&bme_data)) {
	                bme_data.last_update = now;
	            }

	            gps_data = gps_get_last_position();
	            if(gps_data.valid) {
	                gps_last_update = now;
	            }
	        }

	        if(now - last_display_update > 1000) {
	            last_display_update = now;
	            display_update();
	        }

	        if(net_initialized) {
	            http_server_process();
	        }

	        cli_process();
	    }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
