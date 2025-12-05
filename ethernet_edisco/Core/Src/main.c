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
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

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
    .ip = {192, 168, 1, 177},      // Static IP fallback
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
static uint32_t display_last_update = 0;

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
".badge{display:inline-block;padding:3px 8px;border-radius:4px;font-size:0.8em;font-weight:bold;}"
".fix-ok{background:#00aa00;color:white;}"
".fix-no{background:#ff4444;color:white;}"
"#timestamp{text-align:center;color:#888;font-size:0.9em;margin-top:10px;}"
"</style></head><body>"
"<div class='container'>"
"<h1>STM32 Device Status</h1>"
"<div class='card'>"
"<h2>GPS Position</h2>"
"<div class='row'><span class='label'>Latitude:</span><span class='value'>--</span></div>"
"<div class='row'><span class='label'>Longitude:</span><span class='value'>--</span></div>"
"<div class='row'><span class='label'>Fix Status:</span><span class='badge fix-no'>NO FIX</span></div>"
"<div class='row'><span class='label'>Satellites:</span><span class='value'>--</span></div>"
"<div class='row'><span class='label'>UTC Time:</span><span class='value'>--</span></div>"
"</div>"
"<div class='card'>"
"<h2>Environment Sensors</h2>"
"<div class='row'><span class='label'>Temperature:</span><span class='value'>--</span> °C</div>"
"<div class='row'><span class='label'>Pressure:</span><span class='value'>--</span> hPa</div>"
"<div class='row'><span class='label'>Humidity:</span><span class='value'>--</span> %</div>"
"</div>"
"<div id='timestamp'>Connection error</div>"
"</div>"
"</body></html>";

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
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

void wizchip_deselect(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

uint8_t wiz_spi_readbyte(void) {
    uint8_t byte;
    HAL_SPI_Receive(&hspi2, &byte, 1, HAL_MAX_DELAY);
    return byte;
}

void wiz_spi_writebyte(uint8_t byte) {
    HAL_SPI_Transmit(&hspi2, &byte, 1, HAL_MAX_DELAY);
}

void network_init(void) {
    // Reset W5500
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(500);
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(500);

    // Init socket buffers
    uint8_t memsize[8] = {2,2,2,2,2,2,2,2};
    if(wizchip_init(memsize, memsize) == -1) {
        ili9341_draw_text(10, 20, "W5500 Init FAIL!", &font6x8, 0xF800, 0x0000);
        while(1);
    }

    // Set network info
    setnetinfo(&gWIZNETINFO);
    net_initialized = 1;
}

/* --- HTTP server --- */
void http_server_process(void) {
    uint8_t sn = HTTP_SOCKET;
    uint8_t status = get_socket_status(sn);

    switch(status) {
        case W5500_SR_SOCK_ESTABLISHED: {
            uint16_t size = W5500_READ_REG16(W5500_Sn_RX_RSR0(sn));
            if(size > 0) {
                if(size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
                recv_socket(sn, rx_tx_buf, size);
                rx_tx_buf[size] = '\0';

                if(strncmp((char*)rx_tx_buf, "GET /status", 11) == 0) {
                    char json_buf[600];
                    uint32_t now = HAL_GetTick();
                    float gps_age = gps_last_update ? (float)(now - gps_last_update)/1000.0f : 999.9f;
                    float env_age = env_last_update ? (float)(now - env_last_update)/1000.0f : 999.9f;

                    char time_str[32];
                    format_utc_time(gps_data.year, gps_data.month, gps_data.day,
                                   gps_data.hour, gps_data.min, gps_data.sec,
                                   time_str, sizeof(time_str));

                    snprintf(json_buf, sizeof(json_buf),
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
                } else {
                    char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
                    send_socket(sn, (uint8_t*)header, strlen(header));
                    send_socket(sn, (uint8_t*)index_html, strlen(index_html));
                }

                // Disconnect client (keep socket LISTENING)
                disconnect_socket(sn);
            }
            break;
        }
        case W5500_SR_SOCK_LISTEN:
            // nothing, just waiting for connection
            break;
        case W5500_SR_SOCK_INIT:
            listen_socket(sn);
            break;
        case W5500_SR_SOCK_CLOSED:
            socket(sn, 0x01, HTTP_PORT, 0);
            listen_socket(sn);
            break;
    }
}

/* --- Display update --- */
void display_update(void) {
    char buf[50];

    printf(buf, sizeof(buf), "IP: %d.%d.%d.%d", gWIZNETINFO.ip[0], gWIZNETINFO.ip[1],
             gWIZNETINFO.ip[2], gWIZNETINFO.ip[3]);
    ili9341_draw_text(10, 10, buf, &font6x8, 0x07E0, 0x0000);

    printf(buf, sizeof(buf), "GW: %d.%d.%d.%d", gWIZNETINFO.gw[0], gWIZNETINFO.gw[1],
             gWIZNETINFO.gw[2], gWIZNETINFO.gw[3]);
    ili9341_draw_text(10, 20, buf, &font6x8, 0xFFFF, 0x0000);

    printf(buf, sizeof(buf), "T: %.1fC  H: %.1f%%  P: %.1f hPa",
             bme_data.temperature, bme_data.humidity, bme_data.pressure);
    ili9341_draw_text(10, 50, buf, &font6x8, 0xFFFF, 0x0000);

    printf(buf, sizeof(buf), "Lat: %.5f  Lon: %.5f", gps_data.lat_deg, gps_data.lon_deg);
    ili9341_draw_text(10, 90, buf, &font6x8, 0xFFFF, 0x0000);

    printf(buf, sizeof(buf), "Sats: %d  Fix: %d", gps_data.sats, gps_data.fix);
    ili9341_draw_text(10, 110, buf, &font6x8,
                     gps_data.fix >= 2 ? 0x07E0 : 0xF800, 0x0000);
}


/* UART Callback for GPS -----------------------------------------------------*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart->Instance == USART1) {
        nmea_push_chunk(&gps_rx_byte, 1);
        HAL_UART_Receive_IT(&huart1, &gps_rx_byte, 1);
    }
}

void w5500_diagnostic_test(void)
{
    char buf[64];
    uint16_t y_pos = 140;

    ili9341_draw_text(10, y_pos, "=== W5500 Diagnostics ===", &font6x8, 0xFFFF, 0x0000);
    y_pos += 10;

    // Test 1: Read Version Register
    uint8_t version = W5500_READ_REG(0x0039);
    snprintf(buf, sizeof(buf), "Version: 0x%02X", version);
    ili9341_draw_text(10, y_pos, buf, &font6x8,
                     version == 0x04 ? 0x07E0 : 0xF800, 0x0000);
    y_pos += 10;

    // Test 2: Read Mode Register
    uint8_t mr = W5500_READ_REG(W5500_MR);
    snprintf(buf, sizeof(buf), "MR: 0x%02X", mr);
    ili9341_draw_text(10, y_pos, buf, &font6x8, 0xFFFF, 0x0000);
    y_pos += 10;

    // Test 3: Verify MAC Address
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = W5500_READ_REG(W5500_SHAR0 + i);
    }
    snprintf(buf, sizeof(buf), "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ili9341_draw_text(10, y_pos, buf, &font6x8, 0x07E0, 0x0000);
    y_pos += 10;

    // Test 4: Verify IP Address
    uint8_t ip[4];
    for (int i = 0; i < 4; i++) {
        ip[i] = W5500_READ_REG(W5500_SIPR0 + i);
    }
    snprintf(buf, sizeof(buf), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    ili9341_draw_text(10, y_pos, buf, &font6x8, 0x07E0, 0x0000);
    y_pos += 10;

    // Test 5: Check PHY Status
    uint8_t phycfg = W5500_READ_REG(W5500_PHYCFGR);
    snprintf(buf, sizeof(buf), "PHY: 0x%02X %s", phycfg,
             (phycfg & 0x01) ? "LINK UP" : "LINK DOWN");
    ili9341_draw_text(10, y_pos, buf, &font6x8,
                     (phycfg & 0x01) ? 0x07E0 : 0xF800, 0x0000);
    y_pos += 10;

    // Test 6: Socket 0 Buffer Sizes
    uint8_t tx_size = W5500_READ_REG(W5500_Sn_TXBUF_SIZE(0));
    uint8_t rx_size = W5500_READ_REG(W5500_Sn_RXBUF_SIZE(0));
    snprintf(buf, sizeof(buf), "S0 Buf: TX=%dKB RX=%dKB", tx_size, rx_size);
    ili9341_draw_text(10, y_pos, buf, &font6x8, 0xFFFF, 0x0000);
    y_pos += 10;

    // Test 7: Socket 0 Status
    uint8_t s0_sr = W5500_READ_REG(W5500_Sn_SR(0));
    const char* status_str;
    switch(s0_sr) {
        case 0x00: status_str = "CLOSED"; break;
        case 0x13: status_str = "INIT"; break;
        case 0x14: status_str = "LISTEN"; break;
        case 0x17: status_str = "ESTABLISHED"; break;
        default: status_str = "UNKNOWN"; break;
    }
    snprintf(buf, sizeof(buf), "S0 Status: 0x%02X (%s)", s0_sr, status_str);
    ili9341_draw_text(10, y_pos, buf, &font6x8, 0xFFFF, 0x0000);
    y_pos += 10;

    ili9341_draw_text(10, y_pos, "=========================", &font6x8, 0xFFFF, 0x0000);
}

/**
 * Simple W5500 communication test
 * Returns 1 if W5500 responds correctly, 0 otherwise
 */
int w5500_quick_test(void)
{
    // Test: Write and read back a value
    uint8_t test_val = 0xAA;
    W5500_WRITE_REG(W5500_RTR0, test_val);  // Retry Time Register (safe to write)
    HAL_Delay(1);
    uint8_t read_val = W5500_READ_REG(W5500_RTR0);

    if (read_val != test_val) {
        return 0;  // Communication failed
    }

    // Test: Write and read different value
    test_val = 0x55;
    W5500_WRITE_REG(W5500_RTR0, test_val);
    HAL_Delay(1);
    read_val = W5500_READ_REG(W5500_RTR0);

    if (read_val != test_val) {
        return 0;  // Communication failed
    }

    // Restore default value
    W5500_WRITE_REG(W5500_RTR0, 0x07);
    W5500_WRITE_REG(W5500_RTR0 + 1, 0xD0);

    return 1;  // Success
}


/* Display Update ------------------------------------------------------------*/
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
	HAL_Init();
	    SystemClock_Config();

	    MX_GPIO_Init();
	    MX_I2C1_Init();
	    MX_SPI1_Init();
	    MX_SPI2_Init();
	    MX_USART1_UART_Init();

	    ili9341_init(&hspi1);
	    ili9341_fill_screen(0x0000);
	    ili9341_draw_text(10, 10, "Initializing...", &font6x8, 0xFFFF, 0x0000);
	    HAL_Delay(200);
	    uint8_t mr = W5500_READ_REG(0x0000);
	    ili9341_draw_text(10, 140, mr==0?"MR OK":"MR FAIL", &font6x8, 0x07E0, 0x0000);
	    HAL_Delay(400);


	    ili9341_draw_text(10, 30, "Init W5500...", &font6x8, 0xFFFF, 0x0000);
	    HAL_Delay(100);
	    uint8_t memsize[8] = {2,2,2,2,2,2,2,2};
	    if(wizchip_init(memsize, memsize) != 0) {
	        ili9341_draw_text(10, 40, "W5500 Init FAIL!", &font6x8, 0xF800, 0x0000);
	    	HAL_Delay(500);
	        while(1);
	    }

	    // Set network info
	    setnetinfo(&gWIZNETINFO);
	    net_initialized = 1;

	    w5500_diagnostic_test();
	    HAL_Delay(2000);

	    uint8_t sn = 0;
	    int ret = socket(sn, 0x01, 80, 0);  // TCP, port 80

	    char buf[50];
	    if (ret == 0) {
	        snprintf(buf, sizeof(buf), "Socket OPEN: SUCCESS");
	        ili9341_draw_text(10, 250, buf, &font6x8, 0x07E0, 0x0000);

	        // Check status
	        uint8_t status = get_socket_status(sn);
	        snprintf(buf, sizeof(buf), "Status: 0x%02X", status);
	        ili9341_draw_text(10, 260, buf, &font6x8, 0xFFFF, 0x0000);
	        // Should show 0x13 (INIT)
	    } else {
	        snprintf(buf, sizeof(buf), "Socket OPEN FAILED: %d", ret);
	        ili9341_draw_text(10, 250, buf, &font6x8, 0xF800, 0x0000);
	    }

	    ret = listen_socket(sn);
	    if (ret == 0) {
	        snprintf(buf, sizeof(buf), "Socket LISTEN: SUCCESS");
	        ili9341_draw_text(10, 270, buf, &font6x8, 0x07E0, 0x0000);

	        uint8_t status = get_socket_status(sn);
	        snprintf(buf, sizeof(buf), "Status: 0x%02X", status);
	        ili9341_draw_text(10, 280, buf, &font6x8, 0xFFFF, 0x0000);
	        // Should show 0x14 (LISTEN)
	    } else {
	        snprintf(buf, sizeof(buf), "Socket LISTEN FAILED: %d", ret);
	        ili9341_draw_text(10, 270, buf, &font6x8, 0xF800, 0x0000);
	    }

	    // Init sensors
	    bme280_init(&hi2c1);
	    nmea_parser_init();
	    HAL_UART_Receive_IT(&huart1, &gps_rx_byte, 1);

	    ili9341_draw_text(10, 70, "System Ready", &font6x8, 0x07E0, 0x0000);
	    HAL_Delay(200);

	    while(1)
	    {
	        if(net_initialized) http_server_process();

	        uint32_t now = HAL_GetTick();
	        if(nmea_process()) { nmea_get_position(&gps_data); gps_last_update = now; }
	        if(now - env_last_update > 1000) { if(bme280_read(&bme_data)) env_last_update = now; }

	        if(now - display_last_update > 500) { display_update(); display_last_update = now; }

	        HAL_Delay(5); // small delay to avoid tight loop
	    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
