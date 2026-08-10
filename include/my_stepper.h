#ifndef MY_STEPPER_H_
#define MY_STEPPER_H_

#include <avr/io.h>
#include <stdint.h>

// 핀 구조체
typedef struct {
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    uint8_t pin;
} stepper_pin_t;

// 스텝모터 구조체
typedef struct {
    stepper_pin_t in1;
    stepper_pin_t in2;
    stepper_pin_t in3;
    stepper_pin_t in4;
} stepper_t;

// 함수
void stepper_init(stepper_t *motor);
void stepper_step(stepper_t *motor, uint8_t step);
void stepper_rotate(stepper_t *motor, uint16_t steps, uint8_t delay_ms);
void stepper_rotate_reverse(stepper_t *motor, uint16_t steps, uint8_t delay_ms);

#endif
