/**
 * test_rfid_stepper.c
 * [TEST] RC522 RFID 카드 UID 읽기 + 스텝모터 게이트 개폐 테스트 코드
 *
 * 초음파 센서/LCD 없이 RFID 인증만으로 게이트가 정상 개폐되는지
 * 확인하기 위해 작성한 테스트입니다. 최종 코드는 src/main.c 를 참고하세요.
 *
 * 빌드:
 *   avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os \
 *           -o rfid-test.elf test_rfid_stepper.c my_rfid.c my_spi.c \
 *              my_uart.c my_stepper.c -I../include
 *   avr-objcopy -O ihex rfid-test.elf rfid-test.hex
 *   avrdude -c arduino -p m328p -P /dev/ttyUSB0 -b 115200 \
 *           -U flash:w:rfid-test.hex
 */

#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>
#include "my_spi.h"
#include "my_rfid.h"
#include "my_uart.h"
#include "my_stepper.h"

#define LED_DDR   DDRD
#define LED_PORT  PORTD
#define LED_PIN   PD7

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

typedef enum {
    GATE_CLOSED = 0,
    GATE_OPEN   = 1
} gate_state_t;

/* ─────────────────────────────────────────
   헬퍼
──────────────────────────────────────────*/
static void print_uid(const rfid_uid_t *uid)
{
    uart_print("UID: ");
    for (uint8_t i = 0; i < uid->size; i++) {
        if (i > 0) uart_putchar(':');
        printf("%02X", uid->bytes[i]);
    }
    uart_print("\r\n");
}

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

/* ─────────────────────────────────────────
   메인
──────────────────────────────────────────*/
int main(void)
{
    uart_init();
    stdout = uart_get_stdout();

    LED_DDR  |=  (1 << LED_PIN); // LED 출력 설정
    LED_PORT &= ~(1 << LED_PIN); // 처음엔 OFF

    uart_print("\n===========================\n");
    uart_print(" RC522 RFID UID Reader\n");
    uart_print(" SS=PB2(D10), RST=PB1(D9)\n");
    uart_print("===========================\n\n");

    /* SS 핀: PB2 (D10) */
    spi_cs_t rfid_cs = {
        .ddr  = &DDRB,
        .port = &PORTB,
        .pin  = PB2
    };
    rfid_init(rfid_cs);

    stepper_t motor = {
        .in1 = { &DDRD, &PORTD, PD5 }, // D5
        .in2 = { &DDRD, &PORTD, PD4 }, // D4
        .in3 = { &DDRD, &PORTD, PD3 }, // D3
        .in4 = { &DDRD, &PORTD, PD2 }  // D2
    };
    stepper_init(&motor);

    uint8_t version = rfid_get_version();
    printf("RC522 Version: 0x%02X", version);
    uart_print((version == 0x91 || version == 0x92) ? " [OK]\n" : " [WARN]\n");
    uart_print("Ready - tap a card...\n");

    rfid_uid_t uid_current;
    rfid_uid_t uid_last;
    memset(&uid_last, 0, sizeof(uid_last));
    uint8_t same_count = 0;
    gate_state_t gate_state = GATE_CLOSED;

    while (1) {
        LED_PORT &= ~(1 << LED_PIN); // 기본 OFF
        memset(&uid_current, 0, sizeof(uid_current));
        rfid_status_t status = rfid_read_uid(&uid_current);

        if (status == RFID_OK) {
            if (!uid_equal(&uid_current, &uid_last)) {
                print_uid(&uid_current);

                if (is_user_uid(&uid_current)) {
                    // 사용자 카드/키체인
                    if (gate_state == GATE_CLOSED) {
                        LED_PORT |= (1 << LED_PIN); // 인식 표시
                        stepper_rotate(&motor, 512, 2); // 90도 열기
                        gate_state = GATE_OPEN;
                        _delay_ms(1000);
                    }
                }
                else if (is_manager_uid(&uid_current)) {
                    // 관리자 카드/키체인
                    if (gate_state == GATE_OPEN) {
                        LED_PORT |= (1 << LED_PIN); // 인식 표시
                        stepper_rotate_reverse(&motor, 512, 2); // 90도 닫기
                        gate_state = GATE_CLOSED;
                        _delay_ms(1000);
                    }
                }

                uid_last = uid_current;
                same_count = 0;
            } else if (++same_count >= 10) {
                memset(&uid_last, 0, sizeof(uid_last));
                same_count = 0;
            }
        } else if (status == RFID_ERROR) {
            uart_print("[ERROR] Communication error\n");
        }

        _delay_ms(100);
    }
    return 0;
}
