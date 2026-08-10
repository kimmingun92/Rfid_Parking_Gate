#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

// buzzer.h
// Timer0 OC0A 하드웨어 토글 방식 부저 라이브러리
//
// 주의: 부저는 반드시 PD6 (OC0A) 에 연결
// Timer0의 하드웨어 출력 핀이 고정

// 부저 초기화
void buzzer_init(uint8_t pin);

// 부저 시작
void buzzer_begin(uint16_t freq_hz);

// 부저 정지
void buzzer_stop(void);

#endif /* BUZZER_H */
