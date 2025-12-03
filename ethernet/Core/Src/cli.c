/**
 * @file cli.c
 * @brief Command Line Interface for network diagnostics
 */

#include "cli.h"
#include "usart.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include "bme.h"
#include "gps.h"
#include <string.h>
#include <stdio.h>

static uint32_t gps_last_update = 0;
static uint32_t env_last_update = 0;

#define CLI_BUF_SIZE 128
static char cli_buffer[CLI_BUF_SIZE];
static uint8_t cli_index = 0;

extern wiz_NetInfo gWIZNETINFO;
extern bme280_data_t bme_data;
extern gps_pos_t gps_data;

static void cli_print(const char* str) {
    HAL_UART_Transmit(&huart1, (uint8_t*)str, strlen(str), 1000);
}

static void cli_println(const char* str) {
    cli_print(str);
    cli_print("\r\n");
}

/**
 * @brief NET command - Display network parameters and status
 */
static void cmd_net(void) {
    char buf[100];
    wiz_NetInfo netinfo;

    cli_println("\r\n=== Network Status ===");

    // Get current network info
    wizchip_getnetinfo(&netinfo);

    // MAC Address
    printf(buf, "MAC:  %02X:%02X:%02X:%02X:%02X:%02X",
            netinfo.mac[0], netinfo.mac[1], netinfo.mac[2],
            netinfo.mac[3], netinfo.mac[4], netinfo.mac[5]);
    cli_println(buf);

    // IP Address
    printf(buf, "IP:   %d.%d.%d.%d",
            netinfo.ip[0], netinfo.ip[1], netinfo.ip[2], netinfo.ip[3]);
    cli_println(buf);

    // Subnet Mask
    printf(buf, "Mask: %d.%d.%d.%d",
            netinfo.sn[0], netinfo.sn[1], netinfo.sn[2], netinfo.sn[3]);
    cli_println(buf);

    // Gateway
    printf(buf, "GW:   %d.%d.%d.%d",
            netinfo.gw[0], netinfo.gw[1], netinfo.gw[2], netinfo.gw[3]);
    cli_println(buf);

    // DNS Server
    printf(buf, "DNS:  %d.%d.%d.%d",
            netinfo.dns[0], netinfo.dns[1], netinfo.dns[2], netinfo.dns[3]);
    cli_println(buf);

    // Link Status (simple check - if IP is not 0.0.0.0)
    uint8_t link_up = (netinfo.ip[0] != 0 || netinfo.ip[1] != 0 ||
                       netinfo.ip[2] != 0 || netinfo.ip[3] != 0);
    printf(buf, "Link: %s", link_up ? "UP" : "DOWN");
    cli_println(buf);

    // Uptime
    uint32_t uptime_sec = HAL_GetTick() / 1000;
    uint32_t hours = uptime_sec / 3600;
    uint32_t minutes = (uptime_sec % 3600) / 60;
    uint32_t seconds = uptime_sec % 60;
    printf(buf, "Uptime: %luh %lum %lus", hours, minutes, seconds);
    cli_println(buf);

    cli_println("====================\r\n");
}

/**
 * @brief HELP command - Show available commands
 */
static void cmd_help(void) {
    cli_println("\r\nAvailable Commands:");
    cli_println("  NET    - Show network status");
    cli_println("  STATUS - Show sensor data status");
    cli_println("  REBOOT - Restart device");
    cli_println("  HELP   - Show this message\r\n");
}

/**
 * @brief STATUS command - Show sensor data status
 */
static void cmd_status(void) {
    char buf[100];
    uint32_t now = HAL_GetTick();

    cli_println("\r\n=== Sensor Status ===");

    // GPS status
    float gps_age = (float)(now - gps_last_update) / 1000.0f;
    if(gps_last_update == 0) gps_age = 999.9f;

    printf(buf, "GPS:     Age %.1fs %s",
            gps_age, gps_age > 3.0f ? "(STALE)" : "(OK)");
    cli_println(buf);

    printf(buf, "         Lat: %.5f, Lon: %.5f",
            gps_data.lat_deg, gps_data.lon_deg);
    cli_println(buf);

    printf(buf, "         Fix: %d, Sats: %d",
            gps_data.fix, gps_data.sats);
    cli_println(buf);

    // Sensor status
    float sensor_age = (float)(now - bme_data.last_update) / 1000.0f;
    if(bme_data.last_update == 0) sensor_age = 999.9f;

    printf(buf, "Sensors: Age %.1fs %s",
            sensor_age, sensor_age > 3.0f ? "(STALE)" : "(OK)");
    cli_println(buf);

    printf(buf, "         T: %.1f°C, P: %.1f hPa, H: %.1f%%",
            bme_data.temperature, bme_data.pressure, bme_data.humidity);
    cli_println(buf);

    cli_println("====================\r\n");
}

/**
 * @brief REBOOT command - Software reset
 */
static void cmd_reboot(void) {
    cli_println("\r\nRebooting...\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();
}

/**
 * @brief Parse and execute command
 */
static void cli_execute(void) {
    // Convert to uppercase
    for(int i = 0; i < cli_index; i++) {
        if(cli_buffer[i] >= 'a' && cli_buffer[i] <= 'z') {
            cli_buffer[i] -= 32;
        }
    }

    // Null terminate
    cli_buffer[cli_index] = '\0';

    // Execute command
    if(strcmp(cli_buffer, "NET") == 0) {
        cmd_net();
    }
    else if(strcmp(cli_buffer, "HELP") == 0) {
        cmd_help();
    }
    else if(strcmp(cli_buffer, "STATUS") == 0) {
        cmd_status();
    }
    else if(strcmp(cli_buffer, "REBOOT") == 0) {
        cmd_reboot();
    }
    else if(strlen(cli_buffer) > 0) {
        cli_println("Unknown command. Type HELP for list.");
    }

    // Reset buffer
    cli_index = 0;
    cli_print("> ");
}

/**
 * @brief Process CLI characters
 */
void cli_process(void) {
    uint8_t ch;

    // Non-blocking receive
    if(HAL_UART_Receive(&huart1, &ch, 1, 0) == HAL_OK) {

        if(ch == '\r' || ch == '\n') {
            // Execute command
            cli_print("\r\n");
            cli_execute();
        }
        else if(ch == 0x08 || ch == 0x7F) {
            // Backspace
            if(cli_index > 0) {
                cli_index--;
                cli_print("\b \b");
            }
        }
        else if(ch >= 32 && ch <= 126) {
            // Printable character
            if(cli_index < CLI_BUF_SIZE - 1) {
                cli_buffer[cli_index++] = ch;
                HAL_UART_Transmit(&huart1, &ch, 1, 10);
            }
        }
    }
}

/**
 * @brief Initialize CLI
 */
void cli_init(void) {
    cli_println("\r\n");
    cli_println("========================================");
    cli_println("  STM32 Network Panel - Lab 7");
    cli_println("  Type HELP for available commands");
    cli_println("========================================");
    cli_print("> ");
}
