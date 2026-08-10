#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include "buzzer.h"

static uint8_t _pin;

// 초기화
void buzzer_init(uint8_t pin)
{
    _pin = pin;
    DDRD  |=  (1 << PD6); // PD6 출력 설정
    PORTD &= ~(1 << PD6); // 초기값 LOW

    TCCR0A = 0; // Timer0 제어 레지스터A(타이머 동작 방식 설정) 초기화
    TCCR0B = 0; // Timer0 제어 레지스터B(클럭 소스/분주비 설정) 초기화
    TCNT0  = 0; // Timer0 카운터 값 초기화
}

// 부저 시작
void buzzer_begin(uint16_t freq_hz)
{
    // prescaler=64 기준 OCR0A 계산
    uint8_t ocr = (uint8_t)(F_CPU / (2UL * 64 * freq_hz) - 1);

    TCNT0  = 0;   // Timer0 카운터 초기화
    OCR0A  = ocr; // 계산 값 OCR0A 레지스터에 저장

    TCCR0A = (1 << COM0A0) | (1 << WGM01); // OC0A 토글, CTC
    TCCR0B = (1 << CS01) | (1 << CS00);    // prescaler(분주비) = 64
    // CS02=0, CS01=1, CS00=0 -> prescaler = 64
}

// 부저 정지
void buzzer_stop(void)
{
    TCCR0A = 0; // OC0A 토글, CTC 해제
    TCCR0B = 0; // 클럭 소스 차단 = 타이머 정지
    PORTD &= ~(1 << PD6); // 핀 LOW 고정
}
