# src/

드라이버 구현부(.c) 파일과 최종 통합 메인 코드가 위치합니다. 대응되는 헤더는 `../include/`에 있습니다.

| 파일 | 설명 |
| --- | --- |
| `main.c` | ★ **최종 통합 코드**. 초음파(차량 감지) + RFID(개폐 권한) + 스텝모터(차단바) + LCD(상태 안내) + 부저(경고음) + LED(카드 인식 표시)를 하나의 상태 머신으로 통합한 완성 버전 |
| `my_spi.c` | SPCR/SPSR 레지스터 직접 제어로 Hardware SPI 마스터 모드 구현. CS 핀은 `spi_cs_t` 구조체로 소프트웨어 제어 |
| `my_rfid.c` | RC522 레지스터 read/write, `rc522_request`(REQA) → `rc522_anticoll`(Anticollision) 순서로 카드 UID 읽기 + XOR 체크섬 검증 |
| `my_uart.c` | UART0 레지스터 설정, `printf`가 UART로 출력되도록 `FILE` 스트림 콜백 연결 |
| `my_stepper.c` | 4단계 여자 시퀀스(`stepSeq`) 테이블 기반 정방향/역방향 스텝모터 회전 |
| `buzzer.c` | Timer0 CTC 모드 + OC0A 하드웨어 토글로 주파수 발생 |
| `ultrasonic.c` | TRIG 펄스 발생 후 ECHO HIGH 유지 시간을 측정해 거리(cm)로 환산 |
| `my_i2c.c` | ATmega328P TWI(I2C) 레지스터(TWBR/TWCR/TWDR) 직접 제어. 100kHz 기준 START/STOP/WRITE/READ 구현 |
| `lcd1602_i2c.c` | PCF8574 I2C 백팩을 통한 LCD1602 4비트 모드 제어. `my_i2c.c` 위에서 동작 |

## 의존 관계

```
main.c ──▶ ultrasonic.h, buzzer.h, my_spi.h, my_rfid.h, my_uart.h,
           my_stepper.h, my_i2c.h, lcd1602_i2c.h
my_rfid.c     ──▶ my_spi.h
lcd1602_i2c.c ──▶ my_i2c.h
my_uart.c     ──▶ stdout 스트림을 통해 printf와 연동
```
