#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include "ultrasonic.h"

static uint8_t _trig; // TRIG 핀 번호 저장
static uint8_t _echo; // ECHO 핀 번호 저장

// 초기화
void ultrasonic_init(uint8_t trig_pin, uint8_t echo_pin)
{
    _trig = trig_pin;
    _echo = echo_pin;

    // DDRC -> 방향 레지스터 (1=출력, 0=입력)
    DDRC |=  (1 << _trig); // TRIG 출력
    DDRC &= ~(1 << _echo); // ECHO 입력
    PORTC &= ~(1 << _trig); // TRIG 초기값 LOW
}

// 거리 측정
uint16_t ultrasonic_read_cm(void)
{
    // 10us TRIG 펄스 (측정 시작 신호)
    PORTC |=  (1 << _trig); // HIGH
    _delay_us(10);
    PORTC &= ~(1 << _trig); // LOW

    // ECHO HIGH 대기 (타임아웃: 30ms)
    uint16_t timeout = 30000;
    while (!(PINC & (1 << _echo))) {
        if (--timeout == 0) return 9999; // 측정 실패
        _delay_us(1);
    }

    // ECHO HIGH 유지 시간 측정
    uint16_t count = 0;
    while (PINC & (1 << _echo)) {
        count++;
        _delay_us(1);
        if (count > 30000) return 9999; // 측정 실패
    }

    // 거리(cm) = 시간(us) / 59 (근사값)
    // 거리 = 시간 * 음속 / 2 = count * 0.017 = count / 58.8(59)
    return count / 59;
}
