/**
 * main.c
 * RFID 보안 인증 기반 자동 주차 관제 시스템 - 최종 통합 코드
 *
 * 초음파(차량 감지) + RFID(개폐 권한) + 스텝모터(차단바) + LCD(상태 안내)
 * + 부저(경고음) + LED(카드 인식 표시)를 하나의 상태 머신으로 통합한
 * 최종 동작 코드입니다.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>

#include "ultrasonic.h"
#include "buzzer.h"
#include "my_spi.h"
#include "my_rfid.h"
#include "my_uart.h"
#include "my_stepper.h"
#include "my_i2c.h"
#include "lcd1602_i2c.h"

/* ═══════════════════════════════════════════
   핀 설정
   TRIG   : PC0 (A0)
   ECHO   : PC1 (A1)
   BUZZER : PD6 (D6)  ← OC0A 고정
   MOTOR  : PD2~PD5
   LED    : PD7 (D7)
   SS     : PB2 (D10) ← RFID SS
   RST    : PB1 (D9)
   SCK    : PB5 (D13)
   MOSI   : PB3 (D11)
   MISO   : PB4 (D12)
   SDA(I2C): PC4 (A4)
   SCL(I2C): PC5 (A5)
═══════════════════════════════════════════*/
#define TRIG_PIN      PC0
#define ECHO_PIN      PC1
#define BUZZER_PIN    PD6

#define THRESHOLD_CM  5
#define BEEP_HZ       1000

#define LED_DDR       DDRD
#define LED_PORT      PORTD
#define LED_PIN       PD7

/* ═══════════════════════════════════════════
   LCD 화면 상태
═══════════════════════════════════════════*/
typedef enum {
    LCD_SCREEN_WELCOME = 0,
    LCD_SCREEN_DETECTED,
    LCD_SCREEN_CARD_OK,
    LCD_SCREEN_KEYCHAIN_HINT,
    LCD_SCREEN_CLOSED
} lcd_screen_t;

/* ═══════════════════════════════════════════
   등록된 카드 UID
   myCard1, myCard2       -> 사용자 카드 2장
   managerCard, managerKey -> 관리자 카드 / 키체인
═══════════════════════════════════════════*/
static const rfid_uid_t myCard1 = {
    .bytes = {0xED, 0x89, 0x6D, 0x05},
    .size  = 4
};

static const rfid_uid_t myCard2 = {
    .bytes = {0xCC, 0xEF, 0x4B, 0x06},
    .size  = 4
};

static const rfid_uid_t managerCard = {
    .bytes = {0x0D, 0x07, 0x7B, 0x05},
    .size  = 4
};

static const rfid_uid_t managerKey = {
    .bytes = {0x22, 0x29, 0x0D, 0x06},
    .size  = 4
};

/* ═══════════════════════════════════════════
   게이트 상태
═══════════════════════════════════════════*/
typedef enum {
    GATE_CLOSED = 0,
    GATE_OPEN   = 1
} gate_state_t;

/* ═══════════════════════════════════════════
   LCD 화면 출력 함수
═══════════════════════════════════════════*/
static void lcd_show(lcd_screen_t screen)
{
    lcd_clear();
    switch (screen) {
        case LCD_SCREEN_WELCOME:
            lcd_gotoxy(0, 0); lcd_print("Welcome to");
            lcd_gotoxy(0, 1); lcd_print("parking zone");
            break;
        case LCD_SCREEN_DETECTED:
            lcd_gotoxy(0, 0); lcd_print("car detected!");
            lcd_gotoxy(0, 1); lcd_print("please card tag");
            break;
        case LCD_SCREEN_CARD_OK:
            lcd_gotoxy(0, 0); lcd_print("card detected!");
            lcd_gotoxy(0, 1); lcd_print("door open!");
            break;
        case LCD_SCREEN_KEYCHAIN_HINT:
            lcd_gotoxy(0, 0); lcd_print("tag keychain");
            lcd_gotoxy(0, 1); lcd_print("to close door");
            break;
        case LCD_SCREEN_CLOSED:
            lcd_gotoxy(0, 0); lcd_print("Door closed!");
            lcd_gotoxy(0, 1); lcd_print("Thank you!");
            break;
    }
}

/* ═══════════════════════════════════════════
   헬퍼 함수
═══════════════════════════════════════════*/
static void print_uid(const rfid_uid_t *uid)
{
    uart_print("UID: ");
    // UID: ED:89:6D:05 형태로 시리얼 모니터에 출력
    for (uint8_t i = 0; i < uid->size; i++) {
        if (i > 0) uart_putchar(':');
        printf("%02X", uid->bytes[i]);
    }
    uart_print("\r\n");
}

// 두 UID가 같은지 비교. 다르면 0, 같으면 1 반환
static uint8_t uid_equal(const rfid_uid_t *a, const rfid_uid_t *b)
{
    if (a->size != b->size) return 0;
    return (memcmp(a->bytes, b->bytes, a->size) == 0);
}

static uint8_t is_user_uid(const rfid_uid_t *uid)
{
    return uid_equal(uid, &myCard1) || uid_equal(uid, &myCard2);
}

static uint8_t is_manager_uid(const rfid_uid_t *uid)
{
    return uid_equal(uid, &managerCard) || uid_equal(uid, &managerKey);
}

/* ═══════════════════════════════════════════
   메인
═══════════════════════════════════════════*/
int main(void)
{
    // UART 초기화
    uart_init();
    stdout = uart_get_stdout();

    // LED 초기화
    LED_DDR  |=  (1 << LED_PIN);
    LED_PORT &= ~(1 << LED_PIN); // 초기 OFF

    // 초음파 + 부저 초기화
    ultrasonic_init(TRIG_PIN, ECHO_PIN);
    buzzer_init(BUZZER_PIN);

    // I2C + LCD 초기화
    i2c_init();
    lcd_init();

    // RFID 초기화 (SS = PB2)
    spi_cs_t rfid_cs = {
        .ddr  = &DDRB,
        .port = &PORTB,
        .pin  = PB2 // RFID 칩셀렉 핀 = D10
    };
    rfid_init(rfid_cs);

    // 스텝모터 초기화 (D5, D4, D3, D2)
    stepper_t motor = {
        .in1 = { &DDRD, &PORTD, PD5 },
        .in2 = { &DDRD, &PORTD, PD4 },
        .in3 = { &DDRD, &PORTD, PD3 },
        .in4 = { &DDRD, &PORTD, PD2 }
    };
    stepper_init(&motor);

    uart_print("\n===========================\n");
    uart_print(" 초음파 + RFID 게이트 시스템\n");
    uart_print("===========================\n\n");

    uint8_t version = rfid_get_version();
    printf("RC522 Version: 0x%02X", version);
    uart_print((version == 0x91 || version == 0x92) ? " [OK]\n" : " [WARN]\n");
    uart_print("Ready...\n");

    rfid_uid_t uid_current;
    rfid_uid_t uid_last;
    memset(&uid_last, 0, sizeof(uid_last));
    uint8_t same_count = 0;

    gate_state_t gate_state = GATE_CLOSED;
    lcd_screen_t lcd_current = LCD_SCREEN_WELCOME;
    lcd_show(LCD_SCREEN_WELCOME);

    uint8_t  lcd_toggle     = 0; // 0 = CARD_OK, 1 = KEYCHAIN_HINT
    uint16_t toggle_counter = 0; // 100ms x 20 = 2s

    while (1)
    {
        // 1. 초음파 거리 측정
        uint16_t dist = ultrasonic_read_cm();

        // 2. 부저 + LCD 제어
        if (gate_state == GATE_CLOSED) {
            if (dist < THRESHOLD_CM) { // 5cm 이내 물체 감지
                buzzer_begin(BEEP_HZ);
                if (lcd_current != LCD_SCREEN_DETECTED) {
                    lcd_current = LCD_SCREEN_DETECTED;
                    lcd_show(LCD_SCREEN_DETECTED);
                }
            } else { // 측정 범위 초과
                buzzer_stop();
                if (lcd_current != LCD_SCREEN_WELCOME) {
                    lcd_current = LCD_SCREEN_WELCOME;
                    lcd_show(LCD_SCREEN_WELCOME);
                }
            }
        } else { // 게이트가 열려 있을 때, 2초마다 두 화면을 번갈아 표시
            toggle_counter++;
            if (toggle_counter >= 20) { // 20 x 100ms = 2s
                toggle_counter = 0;
                lcd_toggle ^= 1;
                if (lcd_toggle == 0) {
                    lcd_current = LCD_SCREEN_CARD_OK;
                    lcd_show(LCD_SCREEN_CARD_OK);
                } else {
                    lcd_current = LCD_SCREEN_KEYCHAIN_HINT;
                    lcd_show(LCD_SCREEN_KEYCHAIN_HINT);
                }
            }
        }

        // 3. RFID 카드 태그 확인
        memset(&uid_current, 0, sizeof(uid_current));
        rfid_status_t status = rfid_read_uid(&uid_current);

        if (status == RFID_OK) {
            // 방금 전 카드와 다르면 (새 태그)
            if (!uid_equal(&uid_current, &uid_last)) {
                print_uid(&uid_current);

                if (is_user_uid(&uid_current) && gate_state == GATE_CLOSED) {
                    // 사용자 카드: 부저 OFF + LED ON + 게이트 열기
                    buzzer_stop();
                    LED_PORT |= (1 << LED_PIN);
                    uart_print("카드 인식 - 게이트 열림\r\n");

                    lcd_current = LCD_SCREEN_CARD_OK;
                    lcd_show(LCD_SCREEN_CARD_OK);

                    // 모터 512스텝 정방향 (게이트 열기)
                    // 2048스텝(360도) 기준 90도만 사용하므로 2048/4 = 512스텝
                    stepper_rotate(&motor, 512, 2);
                    gate_state = GATE_OPEN;

                    // LCD 교대 타이머 리셋
                    lcd_toggle     = 0;
                    toggle_counter = 0;

                    _delay_ms(500);
                    LED_PORT &= ~(1 << LED_PIN);
                }
                else if (is_manager_uid(&uid_current) && gate_state == GATE_OPEN) {
                    // 관리자 카드/키체인: LED ON + 모터 닫기
                    LED_PORT |= (1 << LED_PIN);
                    uart_print("키체인 인식 - 게이트 닫힘\r\n");

                    lcd_current = LCD_SCREEN_CLOSED;
                    lcd_show(LCD_SCREEN_CLOSED);

                    // 모터 역방향 (게이트 닫기)
                    stepper_rotate_reverse(&motor, 512, 2);
                    gate_state = GATE_CLOSED;

                    _delay_ms(2000);
                    LED_PORT &= ~(1 << LED_PIN);

                    // 초기 화면 복귀
                    lcd_current = LCD_SCREEN_WELCOME;
                    lcd_show(LCD_SCREEN_WELCOME);
                }

                // 현재 카드를 "마지막 카드"로 저장
                uid_last   = uid_current;
                same_count = 0;
            } else if (++same_count >= 10) {
                // 같은 카드를 계속 대고 있어도 10회 후 초기화해서 재인식 허용
                memset(&uid_last, 0, sizeof(uid_last));
                same_count = 0;
            }
        }

        _delay_ms(100);
    }

    return 0;
}
