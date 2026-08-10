#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdint.h>

// ultrasonic.h
// 측정 범위: 2cm ~ 400cm
// 측정 불가 시 반환값: 9999

// 초음파 센서 초기화
void ultrasonic_init(uint8_t trig_pin, uint8_t echo_pin);

// 거리 측정
uint16_t ultrasonic_read_cm(void);

#endif /* ULTRASONIC_H */
