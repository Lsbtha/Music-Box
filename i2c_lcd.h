#ifndef __I2C_LCD_H
#define __I2C_LCD_H

#include "stm32f4xx_hal.h"

void lcd_init(void);
void lcd_send_cmd(uint8_t cmd);
void lcd_send_data(uint8_t data);
void lcd_send_string(char *str);
void lcd_set_cursor(uint8_t row, uint8_t col);

#endif
