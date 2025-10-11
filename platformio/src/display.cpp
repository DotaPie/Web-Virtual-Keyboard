#include "display.h"
#include "pinout.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "config.h"
#include "globals.h"
#include "storage.h"

SPIClass spi_lcd(FSPI);
Adafruit_ST7735 tft(&spi_lcd, TFT_CS, TFT_DC, TFT_RST);

uint8_t chars_to_delete[MAX_LINES] = {0}; // stores chars from previous display_write_word() for right side

void display_write_word(uint16_t color, Align align, uint32_t line, const char *word)
{
    if (line + 1 > MAX_LINES)
    {
        return;
    }

    tft.setTextColor(color);

    if(align == Align::LEFT)
    {
        tft.setCursor(BORDER_OFFSET, DISPLAY_TOP_HW_OFFSET + (BORDER_OFFSET + (line * NEW_LINE_OFFSET)));
        tft.print(word);
    }
    else if(align == Align::RIGHT)
    {
        if(chars_to_delete[line] != 0)
        {
            tft.fillRect(
                DISPLAY_WIDTH - (BORDER_OFFSET + (chars_to_delete[line] * CHARACTER_WIDTH)), 
                DISPLAY_TOP_HW_OFFSET + (BORDER_OFFSET + (line * NEW_LINE_OFFSET)), 
                BORDER_OFFSET + (chars_to_delete[line] * CHARACTER_WIDTH), 
                NEW_LINE_OFFSET, 
                COLOR_BACKGROUND
            );
        }

        chars_to_delete[line] = strlen(word);

        tft.setCursor(DISPLAY_WIDTH - (BORDER_OFFSET + (strlen(word) * CHARACTER_WIDTH)), DISPLAY_TOP_HW_OFFSET + (BORDER_OFFSET + (line * NEW_LINE_OFFSET)));
        tft.print(word);
    }
}

void display_init()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, LOW);

    spi_lcd.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

    tft.initR(INITR_GREENTAB);
    tft.invertDisplay(true);
    tft.setRotation(3); 
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextSize(1); 
    
    display_write_word(COLOR_THEME, Align::LEFT, 0, "  Web Virtual Keyboard");
    display_write_word(COLOR_THEME, Align::LEFT, 1, "-------------------------");
    display_write_word(COLOR_THEME, Align::LEFT, 2, "SSID");
    display_write_word(COLOR_THEME, Align::LEFT, 3, "User");
    display_write_word(COLOR_THEME, Align::LEFT, 4, "Presets");
    display_write_word(COLOR_THEME, Align::LEFT, 5, "Wi-Fi");

    display_write_word(COLOR_WHITE, Align::RIGHT, 2, WIFI_SSID);
    display_write_word(COLOR_WHITE, Align::RIGHT, 3, MASTER_USER);

        
    char temp_string[8] = "";
    sprintf(temp_string, "%d", getPresetsCount());
    display_write_word(COLOR_WHITE, Align::RIGHT, 4, temp_string);

    display_write_word(COLOR_ERROR, Align::RIGHT, 5, "Disconnected");  
}