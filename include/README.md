# include/

모든 드라이버의 공개 헤더(.h) 파일이 위치합니다. 대응되는 구현부(.c)는 `../src/`에 있습니다.

| 파일 | 설명 |
| --- | --- |
| `my_spi.h` | ATmega328P Hardware SPI 범용 드라이버. `spi_config_t`, `spi_cs_t` 타입과 `spi_init/transaction/read_reg/write_reg` 등 API 선언 |
| `my_rfid.h` | RC522 RFID 리더 드라이버. 레지스터 주소, MIFARE 명령 상수, `rfid_uid_t` 타입, `rfid_init/rfid_read_uid` 등 API 선언 |
| `my_uart.h` | UART0 드라이버. `uart_init/uart_print`, `printf` 연동용 `uart_get_stdout()` 선언 |
| `my_stepper.h` | 28BYJ-48 스텝모터 드라이버. `stepper_t` 타입과 정/역방향 회전 함수 선언 |
| `buzzer.h` | Timer0 OC0A 기반 부저 드라이버. `buzzer_init/begin/stop` 선언 |
| `ultrasonic.h` | HC-SR04 초음파 센서 드라이버. `ultrasonic_init/read_cm` 선언 |
| `my_i2c.h` | I2C(TWI) 저수준 통신 드라이버. `i2c_init/start/stop/write/read_ack/read_nack` 선언 |
| `lcd1602_i2c.h` | LCD1602(PCF8574 I2C 백팩) 제어 드라이버. `LCD_ADDR` 등 핀 매핑 매크로, `lcd_init/clear/gotoxy/print` 등 선언 |

## 의존 관계

```
my_rfid.h     ──▶ my_spi.h
lcd1602_i2c.h ──▶ my_i2c.h   (LCD 제어 명령을 I2C 저수준 통신 위에서 처리)
```

`lcd1602_i2c.c`는 내부적으로 `my_i2c.c`의 `i2c_start/write/stop` 등을 호출해서 PCF8574 I2C 백팩에 4비트 모드 명령을 전송하는 구조이므로, 두 파일은 항상 세트로 함께 사용됩니다 (`src/main.c`에서만 사용, `examples/`의 테스트 코드는 LCD를 쓰지 않습니다).
