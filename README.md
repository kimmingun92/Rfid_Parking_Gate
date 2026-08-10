# RFID 보안 인증 기반 자동 주차 관제 시스템

| | |
|---|---|
| 강의명 | 프로젝트 |
| 기간 | 2026년 4월 6일 → 2026년 4월 8일 |
| 메모 | 모듈 프로젝트 |

## 1. 프로젝트 개요

아파트·빌라 등 공동주택 주차장은 '도로'가 아닌 '사유지'로 분류되어, 무단 주차를 해도 경찰이나 지자체가 강제로 견인하거나 과태료를 부과할 법적 근거가 부족하다. 국민권익위원회가 실시한 설문조사에서 응답자의 98%가 사유지 불법주차 단속 근거가 필요하다고 답했다.

법적 제재가 어려운 사유지 특성상, **입구 단계에서 RFID 인증을 통한 물리적 차단**만이 무단 주차를 방지할 가장 확실한 수단이라고 판단해 이 프로젝트를 진행했다.

### 핵심 포인트

- Arduino/HAL 라이브러리를 전혀 사용하지 않고, **AVR ATmega328P 레지스터를 직접 제어**하는 베어메탈 방식으로 SPI·RFID·UART·스텝모터·부저·초음파 드라이버를 전부 자체 구현
- RC522의 ISO14443 REQA/ANTICOLL 인증 절차를 데이터시트 기준으로 직접 구현
- 초음파 센서(차량 감지) + RFID 인증(개폐 권한) + 스텝모터(물리적 차단바) + LCD(상태 안내)를 하나의 상태 머신으로 통합
- 사용자 카드와 관리자 카드를 구분해 게이트 열림/닫힘을 서로 다른 권한으로 분기 처리

## 2. 시스템 구성

| 부품 | 통신 방식 | 역할 |
|---|---|---|
| RC522 RFID 리더 | SPI | 카드 UID 인식, 인증 |
| RFID 카드/키체인 | - | 사용자 키(입장) / 관리자 키(퇴장·강제개방) |
| 28BYJ-48 스텝모터 + ULN2003 드라이버 | GPIO 4핀 | 차단바(게이트) 개폐 |
| HC-SR04 초음파 센서 | GPIO (Trig/Echo) | 차량 접근 감지 |
| 부저 | Timer0 PWM(OC0A) | 차량 감지 경고음 |
| LCD1602 (I2C) | I2C | 상태 안내 문구 표시 |
| LED | GPIO | 카드 인식 표시 |
| UART | UART0 | 디버깅 로그 출력 (printf 연동) |

### 회로 연결 요약 (최종 통합 코드 `src/main.c` 기준)

| 모듈 | 핀 | 기능 |
|---|---|---|
| RC522 SDA(SS) | D10 (PB2) | SPI 슬레이브 선택 |
| RC522 SCK | D13 (PB5) | SPI 클럭 |
| RC522 MOSI | D11 (PB3) | 마스터→모듈 데이터 출력 |
| RC522 MISO | D12 (PB4) | 모듈→마스터 데이터 입력 |
| RC522 RST | D9 (PB1) | 하드웨어 리셋 |
| ULN2003 IN1~IN4 | D5, D4, D3, D2 | 스텝모터 코일 A/B/C/D 제어 |
| LCD1602 SDA / SCL | A4 / A5 | I2C 통신 |
| 초음파 TRIG / ECHO | A0(PC0) / A1(PC1) | 차량 거리 감지 |
| 부저 | D6 (PD6, OC0A 고정) | Timer0 하드웨어 토글 출력 |
| LED | D7 (PD7) | 카드 인식 표시 |

> 테스트 코드(`examples/`)는 개발 단계별 배선이 최종 통합 코드와 다를 수 있습니다. 각 파일 상단 주석의 핀 정의를 확인하세요.

## 3. 소프트웨어 아키텍처 — 자체 구현 드라이버

Arduino 라이브러리를 쓰지 않고, 각 하드웨어를 레지스터 단위로 제어하는 드라이버를 계층 구조로 직접 설계했다.

- **`my_spi`** — ATmega328P Hardware SPI(SPCR/SPSR) 범용 드라이버. MODE0, 클럭 분주 4(4MHz), MSB First로 설정. 여러 SPI 장치를 확장할 수 있도록 CS 핀을 `spi_cs_t` 구조체로 추상화
- **`my_rfid`** — `my_spi` 위에 쌓은 RC522 전용 레지스터 읽기/쓰기 계층. `rc522_request` → `rc522_anticoll` 순서로 REQA 요청과 Anticollision 절차를 거쳐 카드 UID 4바이트를 읽고 XOR 체크섬으로 검증
- **`my_uart`** — UART0 레지스터(UBRR/UCSR) 직접 설정, `printf`가 UART로 출력되도록 `FILE` 스트림 연결
- **`my_stepper`** — 코일 4핀을 구조체로 묶어 관리하며, 4단계 시퀀스(A→B→C→D)로 여자 순서를 제어. 정방향/역방향 회전 함수 분리
- **`buzzer`** — Timer0 CTC 모드, OC0A 하드웨어 토글 방식으로 주파수 발생 (PD6 고정)
- **`ultrasonic`** — TRIG 10μs 펄스 후 ECHO HIGH 유지 시간을 측정해 `시간(μs)/58` 로 거리(cm) 환산, 타임아웃 처리 포함
- **`my_i2c`** — ATmega328P TWI(I2C) 레지스터(TWBR/TWCR/TWDR) 직접 제어. 16MHz 기준 TWBR=72로 100kHz SCL 생성
- **`lcd1602_i2c`** — `my_i2c` 위에서 동작하는 LCD1602(PCF8574 I2C 백팩) 4비트 모드 제어 드라이버

## 4. 동작 시나리오 (상태 머신)

게이트 상태는 `GATE_CLOSED` / `GATE_OPEN` 두 단계로 관리하고, LCD는 상황에 따라 5가지 화면을 전환한다.

```
[Welcome to parking zone]   (기본 대기 상태)
        │  초음파가 임계거리(5cm) 이내 차량 감지
        ▼
[car detected! please card tag]
        │  사용자 카드 태그 (게이트 CLOSED 상태에서만 반응)
        ▼
[card detected! door open!]  ← 스텝모터 정방향 512스텝 회전, LED ON
        │  2초 주기로 자동 토글
        ▼
[card detected!] ⇄ [tag keychain to close door]
        │  관리자 카드/키체인 태그 (게이트 OPEN 상태에서만 반응)
        ▼
[Door closed! Thank you!]  ← 스텝모터 역방향 512스텝 회전
        │  2초 후
        ▼
다시 [Welcome to parking zone]
```

- **사용자 카드**(개인 카드/키체인 2개 등록): 게이트가 닫혀 있을 때만 태그 인식 → 부저 정지, LED 점등, 게이트 열림
- **관리자 카드**(관리자 카드/키체인 2개 등록): 게이트가 열려 있을 때만 태그 인식 → 게이트 닫힘
- UID가 등록된 두 종류(사용자/관리자) 중 어디에도 속하지 않으면 게이트는 반응하지 않음 (미인가 카드는 무시)
- 동일 UID가 연속으로 인식되는 경우 중복 처리를 막기 위해 `uid_last` 비교 및 `same_count` 타임아웃 로직으로 필터링

## 5. 트러블슈팅

### ① RC522 FIFO 연속 전송 시 통신 오류

- **문제**: RC522 FIFO에 데이터를 전송할 때 바이트마다 CS를 재설정하며 주소를 반복 전송하면, RC522가 이를 새로운 명령으로 오인해 통신 오류가 발생
- **원인**: 데이터시트 재확인 결과, RC522는 CS가 유지되는 동안 첫 바이트만 주소로 해석하고 이후 바이트는 데이터로 처리해야 하는 프로토콜이었음
- **해결**: CS를 한 번만 LOW로 유지한 채, 주소를 1회 전송한 뒤 데이터를 연속으로 전송하도록 `rc522_to_card()`를 수정
- **결과**: FIFO 적재 시 통신 오류 해소, 안정적인 카드 통신 확보

### ② AVR 하드웨어 SS 핀으로 인한 마스터 모드 예기치 않은 해제

- **문제**: AVR Hardware SPI를 마스터 모드로 초기화했음에도, 하드웨어 SS 핀(PB2)이 입력 상태로 남아있어 마스터 모드가 예기치 않게 해제될 위험이 있었음
- **원인**: ATmega328P의 SPI는 SS 핀이 입력이고 LOW로 떨어지면 마스터 모드를 자동 해제하는 하드웨어 특성을 가지고 있음을 확인
- **해결**: 실제 카드 선택(CS)은 별도 GPIO(`spi_cs_t`)로 소프트웨어 제어하되, 하드웨어 SS 핀(PB2)도 항상 출력으로 고정 설정
- **결과**: SPI 마스터 모드가 예기치 않게 풀리는 문제 제거, 통신 안정성 확보

## 6. 결과

- 초음파 센서로 차량 접근을 감지하고, RFID 카드 태깅만으로 게이트가 자동 개폐되는 시스템 구현 완료
- 사용자 카드와 관리자 카드를 구분하여 서로 다른 동작(열기/닫기)이 정상 수행됨을 확인
- LCD에 상황별 안내 문구(대기/차량감지/문열림/키체인 안내/문닫힘)가 상태에 맞게 정상 전환됨을 확인

## 7. 고찰

- SPI, I2C, UART 등 통신 프로토콜을 라이브러리 없이 레지스터 단위로 직접 구현하며 각 프로토콜의 타이밍·모드 설정 원리를 이해함
- RC522의 FIFO 통신 특성을 통해 데이터시트만으로는 파악하기 어려운 실제 하드웨어 동작 방식(연속 전송 시 주소 처리 규칙)을 실습으로 확인함
- 오픈 컬렉터 방식 드라이버(ULN2003)의 전류 증폭 원리와 스텝모터의 여자 시퀀스 회전 원리를 이해함
- 상태 기계(state machine) 설계를 통해 하드웨어 입력(카드 태깅)에 따라 시스템이 예측 가능하게 동작하도록 구조화하는 경험을 쌓음

## 8. 저장소 구조

각 폴더에는 해당 폴더 파일들을 설명하는 `README.md`가 별도로 있습니다.

```
rfid-parking-gate/
├── platformio.ini              # PlatformIO 프로젝트 설정 (env:uno + 테스트용 env 2개)
├── include/
│   ├── README.md
│   ├── ultrasonic.h
│   ├── buzzer.h
│   ├── my_spi.h
│   ├── my_rfid.h
│   ├── my_uart.h
│   ├── my_stepper.h
│   ├── my_i2c.h
│   └── lcd1602_i2c.h
├── src/
│   ├── README.md
│   ├── ultrasonic.c
│   ├── buzzer.c
│   ├── my_spi.c
│   ├── my_rfid.c
│   ├── my_uart.c
│   ├── my_stepper.c
│   ├── my_i2c.c
│   ├── lcd1602_i2c.c
│   └── main.c                  # ★ 최종 통합 메인 (초음파+RFID+스텝모터+LCD+부저+LED)
├── examples/
│   ├── README.md
│   ├── test_buzzer_ultrasonic.c   # 부저 + 초음파 단독 테스트
│   └── test_rfid_stepper.c        # RFID + 스텝모터 단독 테스트 (LCD/초음파 제외)
├── .gitignore
└── README.md
```

- **`src/main.c`**: 최종 통합 코드입니다. 초음파, RFID, 스텝모터, LCD, 부저, LED를 모두 하나의 상태 머신으로 결합한 완성 버전입니다. `platformio.ini`의 기본 환경(`env:uno`)이 이 파일을 빌드합니다.
- **`examples/test_buzzer_ultrasonic.c`**, **`examples/test_rfid_stepper.c`**: 개발 초기 단계에서 기능별로 나눠 검증한 테스트 코드입니다. 각각 전용 PlatformIO 환경으로 빌드합니다.

## 9. 빌드 방법 (PlatformIO)

VSCode + PlatformIO 확장 기준입니다. CLI로도 동일하게 동작합니다.

```bash
# 최종 통합 코드 빌드 (기본 env:uno)
pio run

# 보드 업로드 + 시리얼 모니터
pio run -t upload
pio device monitor

# 테스트 코드 빌드/업로드
pio run -e test_buzzer_ultrasonic
pio run -e test_rfid_stepper -t upload

# 빌드 산출물 정리
pio run -t clean
```

> 이 프로젝트는 Arduino 라이브러리를 쓰지 않고 `int main(void)`를 직접 정의하는 베어메탈 방식이라, `platformio.ini`에 `framework` 항목을 지정하지 않았습니다(`platform = atmelavr`만 사용). Arduino 코어와 함께 쓰면 `main()`이 중복 정의되어 링크 에러가 납니다.

## 10. 등록 카드 UID 변경

`src/main.c` (및 `examples/test_rfid_stepper.c`) 상단의 `myCard1`, `myCard2`, `managerCard`, `managerKey` 배열 값을 실제 보유한 카드의 UID로 교체하면 됩니다. UART(9600bps)로 로그를 확인하면 태그한 카드의 UID가 `UID: XX:XX:XX:XX` 형태로 출력되므로, 이 값을 그대로 복사해서 등록하면 됩니다.
