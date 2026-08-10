# examples/

개발 단계별로 기능을 하나씩 검증하기 위해 작성한 테스트용 `main()` 코드입니다. 최종 완성 코드가 아니라 통합 이전 단계의 동작 확인용이며, 완성 코드는 `../src/main.c` 입니다.

| 파일 | 설명 |
| --- | --- |
| `test_buzzer_ultrasonic.c` | 초음파 센서로 거리를 측정해 임계거리(5cm) 이내면 부저가 울리는지만 검증. LCD/RFID/스텝모터 없음 |
| `test_rfid_stepper.c` | RC522로 카드 UID를 읽어 사용자/관리자 카드를 구분하고, 스텝모터로 게이트를 여닫는지만 검증. 초음파/LCD 없음 |

## 배선 차이 주의

각 테스트는 최종 통합 코드(`src/main.c`)와 핀 배치가 다를 수 있습니다 (예: `test_buzzer_ultrasonic.c`는 TRIG=PD4, ECHO=PD5를 쓰지만 `main.c`는 TRIG=PC0, ECHO=PC1을 씁니다). 실제 배선하기 전에 각 파일 상단의 `#define` 핀 설정을 반드시 확인하세요.

## 빌드 (PlatformIO)

저장소 루트의 `platformio.ini`에 테스트 전용 환경이 정의되어 있습니다. `src/main.c` 대신 해당 테스트 파일만 빌드합니다.

```bash
pio run -e test_buzzer_ultrasonic
pio run -e test_rfid_stepper

# 보드 업로드
pio run -e test_rfid_stepper -t upload
```
