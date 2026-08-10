#include "my_stepper.h"
#include <util/delay.h>

// 4단계 시퀀스
static const uint8_t stepSeq[4][4] = {
    {1, 0, 0, 0}, // 코일1만 on : 스텝0
    {0, 1, 0, 0}, // 코일2만 on : 스텝1
    {0, 0, 1, 0}, // 코일3만 on : 스텝2
    {0, 0, 0, 1}  // 코일4만 on : 스텝3
};

// 핀 출력 설정
void stepper_init(stepper_t *motor) {
    *(motor->in1.ddr) |= (1 << motor->in1.pin);
    *(motor->in2.ddr) |= (1 << motor->in2.pin);
    *(motor->in3.ddr) |= (1 << motor->in3.pin);
    *(motor->in4.ddr) |= (1 << motor->in4.pin);
}

// 한 스텝 출력
// 스텝 번호(0~3)를 받아서 해당 시퀀스대로 핀 출력
void stepper_step(stepper_t *motor, uint8_t step) {
    if (stepSeq[step][0])
        *(motor->in1.port) |=  (1 << motor->in1.pin);
    else
        *(motor->in1.port) &= ~(1 << motor->in1.pin);

    if (stepSeq[step][1])
        *(motor->in2.port) |=  (1 << motor->in2.pin);
    else
        *(motor->in2.port) &= ~(1 << motor->in2.pin);

    if (stepSeq[step][2])
        *(motor->in3.port) |=  (1 << motor->in3.pin);
    else
        *(motor->in3.port) &= ~(1 << motor->in3.pin);

    if (stepSeq[step][3])
        *(motor->in4.port) |=  (1 << motor->in4.pin);
    else
        *(motor->in4.port) &= ~(1 << motor->in4.pin);
}

// 정방향 회전
void stepper_rotate(stepper_t *motor, uint16_t steps, uint8_t delay_ms) {
    for (uint16_t i = 0; i < steps; i++) {
        // i % 4 -> 0,1,2,3,0,1,2,3... 순서로 반복
        stepper_step(motor, i % 4);
        _delay_ms(delay_ms);
    }
}

// 역방향 회전
void stepper_rotate_reverse(stepper_t *motor, uint16_t steps, uint8_t delay_ms)
{
    // steps-1 부터 거꾸로 카운트
    // uint16_t면 i가 0일 때 i--하면 언더플로우로 65535가 되어 무한루프 걸림
    for (int16_t i = steps - 1; i >= 0; i--) {
        // i % 4 -> 3,2,1,0,3,2,1,0... 순서로 반복
        stepper_step(motor, i % 4);
        _delay_ms(delay_ms);
    }
}
