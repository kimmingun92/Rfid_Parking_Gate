#include "lcd1602_i2c.h"
#include <util/delay.h>
#include "my_i2c.h"

static uint8_t lcd_backlight = (1 << BL_BIT); // 기본 ON

static void lcd_write_pcf8574(uint8_t data) {
    i2c_start();
    i2c_write(LCD_ADDR);
    i2c_write(data | lcd_backlight);
    i2c_stop();
}

static void lcd_pulse_enable(uint8_t data) {
    lcd_write_pcf8574(data | (1 << EN_BIT));
    _delay_us(1);
    lcd_write_pcf8574(data & ~(1 << EN_BIT));
    _delay_us(50);
}

static void lcd_send(uint8_t value, uint8_t mode) {
    uint8_t high = (value & 0xF0) | mode;
    uint8_t low  = ((value << 4) & 0xF0) | mode;

    lcd_write_pcf8574(high);
    lcd_pulse_enable(high);
    lcd_write_pcf8574(low);
    lcd_pulse_enable(low);
}

void lcd_command(uint8_t cmd)  { lcd_send(cmd, 0); }
void lcd_data(uint8_t data)    { lcd_send(data, (1 << RS_BIT)); }

void lcd_init(void) {
    _delay_ms(50);
    // 4비트 초기화 시퀀스
    lcd_write_pcf8574(0x30); lcd_pulse_enable(0x30); _delay_ms(5);
    lcd_write_pcf8574(0x30); lcd_pulse_enable(0x30); _delay_us(150);
    lcd_write_pcf8574(0x30); lcd_pulse_enable(0x30);
    lcd_write_pcf8574(0x20); lcd_pulse_enable(0x20); // 4-bit mode 설정

    lcd_command(0x28); // 2 Line, 5x8 Matrix
    lcd_command(0x0C); // Display ON, Cursor OFF
    lcd_command(0x06); // Entry Mode: Increment
    lcd_clear();
}

void lcd_gotoxy(uint8_t x, uint8_t y) {
    uint8_t addr = (y == 0) ? (0x80 + x) : (0xC0 + x);
    lcd_command(addr);
}

void lcd_print(const char* str) {
    while (*str) lcd_data(*str++);
}

void lcd_clear(void) {
    lcd_command(LCD_CLEARDISPLAY);
    _delay_ms(2);
}

// Display on/off control: 0000001DCB
// D = 0; Display off
// C = 0; Cursor off
// B = 0; Blinking off
void lcd_display_on(void) {
    lcd_command(0x0C);   // Display ON, Cursor OFF, Blink OFF
}

void lcd_display_off(void) {
    lcd_command(0x08);   // Display OFF, Cursor OFF, Blink OFF
}

void lcd_backlight_on(void) {
    lcd_backlight = (1 << BL_BIT);
    lcd_write_pcf8574(0x00);
}

void lcd_backlight_off(void) {
    lcd_backlight = 0x00;
    lcd_write_pcf8574(0x00);
}
