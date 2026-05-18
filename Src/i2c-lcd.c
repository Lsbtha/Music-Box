#include "i2c_lcd.h"

extern I2C_HandleTypeDef hi2c2;
#define LCD_ADDR 0x7C

void lcd_send_cmd(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};
    HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDR, data, 2, 100);
}

void lcd_send_data(uint8_t data) {
    uint8_t buf[2] = {0x40, data};
    HAL_I2C_Master_Transmit(&hi2c2, LCD_ADDR, buf, 2, 100);
}

void lcd_init(void) {
    HAL_Delay(50);
    lcd_send_cmd(0x38);
    HAL_Delay(1);
    lcd_send_cmd(0x0C);
    HAL_Delay(1);
    lcd_send_cmd(0x01);
    HAL_Delay(2);
    lcd_send_cmd(0x06);
}

void lcd_send_string(char *str) {
    while (*str) lcd_send_data(*str++);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    lcd_send_cmd(addr);
}
