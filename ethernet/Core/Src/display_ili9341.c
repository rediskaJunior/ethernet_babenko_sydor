#include "display_ili9341.h"
#include "fonts.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>

#define ILI_CS_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)
#define ILI_CS_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define ILI_DC_COMMAND() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define ILI_DC_DATA()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define ILI_RST_LOW()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET)
#define ILI_RST_HIGH()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET)
#define ILI_BL_ON()     HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET)
#define ILI_BL_OFF()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET)

static SPI_HandleTypeDef* spi;

static void ili_write_cmd(uint8_t cmd) {
    ILI_DC_COMMAND();
    ILI_CS_LOW();
    HAL_SPI_Transmit(spi, &cmd, 1, HAL_MAX_DELAY);
    ILI_CS_HIGH();
}
static void ili_write_data(uint8_t *data, uint16_t len) {
    ILI_DC_DATA();
    ILI_CS_LOW();
    HAL_SPI_Transmit(spi, data, len, HAL_MAX_DELAY);
    ILI_CS_HIGH();
}

void ili9341_init(SPI_HandleTypeDef* hspi) {
    spi = hspi;
    ILI_RST_LOW();
    HAL_Delay(10);
    ILI_RST_HIGH();
    HAL_Delay(120);

    ili_write_cmd(0xEF);
    uint8_t d1[] = {0x03,0x80,0x02};
    ili_write_data(d1,3);

    ili_write_cmd(0xCF);
    uint8_t d2[] = {0x00,0xC1,0x30};
    ili_write_data(d2,3);

    ili_write_cmd(0xED);
    uint8_t d3[] = {0x64,0x03,0x12,0x81};
    ili_write_data(d3,4);

    ili_write_cmd(0xE8);
    uint8_t d4[] = {0x85,0x00,0x78};
    ili_write_data(d4,3);

    ili_write_cmd(0xCB);
    uint8_t d5[] = {0x39,0x2C,0x00,0x34,0x02};
    ili_write_data(d5,5);

    ili_write_cmd(0xF7);
    uint8_t d6[] = {0x20};
    ili_write_data(d6,1);

    ili_write_cmd(0xEA);
    uint8_t d7[] = {0x00,0x00};
    ili_write_data(d7,2);

    ili_write_cmd(0xC0);
    uint8_t d8[] = {0x23};
    ili_write_data(d8,1);

    ili_write_cmd(0xC1);
    uint8_t d9[] = {0x10};
    ili_write_data(d9,1);

    ili_write_cmd(0xC5);
    uint8_t d10[] = {0x3e,0x28};
    ili_write_data(d10,2);

    ili_write_cmd(0x36);
    uint8_t orient[] = {0x48};
    ili_write_data(orient,1);

    ili_write_cmd(0x3A);
    uint8_t d11[] = {0x55};
    ili_write_data(d11,1);

    ili_write_cmd(0x11);
    HAL_Delay(120);
    ili_write_cmd(0x29);
    HAL_Delay(20);

    ILI_BL_ON();
}

void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];
    ili_write_cmd(0x2A);
    data[0] = (x0>>8)&0xFF; data[1] = x0 & 0xFF; data[2] = (x1>>8)&0xFF; data[3] = x1 &0xFF;
    ili_write_data(data,4);

    ili_write_cmd(0x2B);
    data[0] = (y0>>8)&0xFF; data[1] = y0 & 0xFF; data[2] = (y1>>8)&0xFF; data[3] = y1 &0xFF;
    ili_write_data(data,4);

    ili_write_cmd(0x2C);
}

void ili9341_blit_pixels(const uint16_t* pixels, size_t count) {
    ILI_DC_DATA();
    ILI_CS_LOW();
    HAL_SPI_Transmit(spi, (uint8_t*)pixels, count*2, HAL_MAX_DELAY);
    ILI_CS_HIGH();
}

void ili9341_fill_screen(uint16_t color) {
    ili9341_set_window(0,0,239,319);
    static uint16_t linebuf[240];
    for (int i=0;i<240;i++) linebuf[i]=color;
    for (int y=0;y<320;y++) {
        ili9341_blit_pixels(linebuf, 240);
    }
}

void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (w == 0 || h == 0) return;
    ili9341_set_window(x,y,x+w-1,y+h-1);
    static uint16_t buf[240];
    int count = w;
    for (int i=0;i<count;i++) buf[i]=color;
    for (int j=0;j<h;j++) {
        ili9341_blit_pixels(buf, count);
    }
}

void ili9341_draw_text(uint16_t x, uint16_t y, const char* text, const font_t* font, uint16_t fg, uint16_t bg) {
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') { cursor_y += font->height; cursor_x = x; continue; }
        if (*p < font->first || *p > font->last) continue;
        int idx = (*p) - font->first;
        const uint8_t* glyph = font->data + idx*font->bytes;
        for (int col=0; col<font->width; ++col) {
            uint8_t colbits = glyph[col];
            uint16_t pixelline[font->height];
            for (int row=0; row<font->height; ++row) {
                bool bit = (colbits >> row) & 1;
                pixelline[row] = bit ? fg : bg;
            }
            ili9341_set_window(cursor_x+col, cursor_y, cursor_x+col, cursor_y+font->height-1);
            ili9341_blit_pixels(pixelline, font->height);
        }
        cursor_x += font->width + 1;
    }
}
