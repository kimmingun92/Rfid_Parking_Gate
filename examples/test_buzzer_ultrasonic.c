/**
 * test_buzzer_ultrasonic.c
 * [TEST] 초음파 센서 + 부저 단독 동작 확인용 테스트 코드
 *
 * 최종 통합 코드가 아닌, 개발 초기 단계에서 초음파 거리 측정과
 * 부저 경고음 연동이 정상 동작하는지 확인하기 위해 작성한 테스트입니다.
 * 최종 코드는 src/main.c 를 참고하세요.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <util/delay.h>
#include "ultrasonic.h"
#include "buzzer.h"

#define TRIG_PIN      4    // PD4
#define ECHO_PIN      5    // PD5
#define BUZZER_PIN    3    // PD3 (OC2B 고정)

#define THRESHOLD_CM  5    // 감지 거리 (cm)
#define BEEP_HZ       1000 // 부저 주파수 (Hz)

int main(void)
{
    ultrasonic_init(TRIG_PIN, ECHO_PIN);
    buzzer_init(BUZZER_PIN);

    while (1)
    {
        uint16_t dist = ultrasonic_read_cm();

        if (dist < THRESHOLD_CM) buzzer_begin(BEEP_HZ);
        else                     buzzer_stop();

        _delay_ms(100);
    }
}
