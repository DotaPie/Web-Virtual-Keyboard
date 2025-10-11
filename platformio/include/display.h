#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define DISPLAY_INIT_FLAG INITR_GREENTAB // this isnt really my display, but it produces no artifacts and is aligned fine left/right at least, is is just shifted 24 pixels above the screen
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 80
#define DISPLAY_TOP_HW_OFFSET 24
#define CHARACTER_WIDTH 6
#define MAX_LINES 6

#define NEW_LINE_OFFSET 12
#define BORDER_OFFSET 5

#define COLOR_BACKGROUND ST77XX_BLACK
#define COLOR_THEME ST77XX_ORANGE
#define COLOR_WHITE ST77XX_WHITE
#define COLOR_OK    ST77XX_GREEN
#define COLOR_ERROR ST77XX_RED

enum Align
{
    LEFT,
    RIGHT
};

void display_init();
void display_write_word(uint16_t color, Align align, uint32_t line, const char *word);

#endif