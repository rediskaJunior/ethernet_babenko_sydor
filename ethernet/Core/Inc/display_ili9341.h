#ifndef INC_DISPLAY_ILI9341_H_
#define INC_DISPLAY_ILI9341_H_

#include "stm32f4xx_hal.h"
#include "fonts.h"
#include <stdint.h>

#define BLACK 0x0000
#define WHITE 0xFFFF

typedef struct {
    SPI_HandleTypeDef* hspi;
} ili9341_ctx_t;

void ili9341_init(SPI_HandleTypeDef* hspi);
void ili9341_fill_screen(uint16_t color);
void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ili9341_draw_text(uint16_t x, uint16_t y, const char* text, const font_t* font, uint16_t fg, uint16_t bg);

void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ili9341_blit_pixels(const uint16_t* pixels, size_t count); // blocking; underneath uses SPI & CS


#endif /* INC_DISPLAY_ILI9341_H_ */
