/**
 * my_rfid.c
 * RC522 RFID 드라이버 구현 — my_spi.h/my_spi.c 사용
 */

#include "my_rfid.h"

/* ─────────────────────────────────────────
   모듈 내부 상태 변수
──────────────────────────────────────────*/
static spi_cs_t _cs; /* rfid_init() 에서 설정 */

/* ═══════════════════════════════════════════
   RC522 레지스터 접근
   RC522 SPI 프로토콜:
     주소 바이트 = (reg << 1) & 0x7E          (쓰기)
     주소 바이트 = ((reg << 1) & 0x7E) | 0x80 (읽기)
═══════════════════════════════════════════*/

/* 레지스터에 값을 쓰는 함수 */
void rc522_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { (reg << 1) & 0x7E, value };
    spi_transaction(_cs, buf, NULL, 2);
}

/* 레지스터 값을 읽는 함수 */
uint8_t rc522_read_reg(uint8_t reg)
{
    // tx[0]=읽기용 주소(최상위비트 1), tx[1]=더미 바이트(0xFF)
    // SPI는 송수신이 동시에 일어나므로 읽을 때도 더미 값 0xFF 전송
    uint8_t tx[2] = { ((reg << 1) & 0x7E) | 0x80, 0xFF };
    uint8_t rx[2];
    // rx[0]은 주소 보낼 때 온 쓰레기값, 실제 데이터는 rx[1]
    spi_transaction(_cs, tx, rx, 2);
    return rx[1];
}

/* 레지스터의 특정 비트만 1로 세팅 (비트 마스킹) */
void rc522_set_bits(uint8_t reg, uint8_t mask)
{
    rc522_write_reg(reg, rc522_read_reg(reg) | mask);
}

/* 레지스터의 특정 비트만 0으로 클리어 */
void rc522_clear_bits(uint8_t reg, uint8_t mask)
{
    rc522_write_reg(reg, rc522_read_reg(reg) & ~mask);
}

/* ═══════════════════════════════════════════
   RC522 초기화 관련 함수
═══════════════════════════════════════════*/

/* RC522에 소프트웨어 리셋 명령 전송 후 50ms 대기 */
void rc522_reset(void)
{
    rc522_write_reg(REG_COMMAND, CMD_SOFT_RESET);
    _delay_ms(50);
}

/* 안테나 출력 활성화
   TX1, TX2 핀(비트 0,1)이 둘 다 켜져 있지 않으면 켜줌
   안테나가 꺼져 있으면 카드 인식 불가 */
void rc522_antenna_on(void)
{
    uint8_t tx = rc522_read_reg(REG_TX_CONTROL);
    if ((tx & 0x03) != 0x03) {
        rc522_set_bits(REG_TX_CONTROL, 0x03);
    }
}

/**
 * @brief SPI + RC522 전체 초기화
 * @param cs SS 핀 (spi_cs_t)
 */
void rfid_init(spi_cs_t cs)
{
    /* SPI 초기화 (MODE0, 4 MHz, MSB first) */
    spi_config_t cfg = SPI_CONFIG_DEFAULT;
    spi_init(&cfg);

    /* CS 핀 초기화 */
    _cs = cs;
    spi_cs_init(_cs);

    /* RST 핀 출력 설정 */
    RFID_RST_DDR |= (1 << RFID_RST_PIN_BIT);

    /* 하드웨어 리셋: LOW -> HIGH */
    RFID_RST_PORT &= ~(1 << RFID_RST_PIN_BIT);
    _delay_ms(10);
    RFID_RST_PORT |=  (1 << RFID_RST_PIN_BIT);
    _delay_ms(50);

    /* 소프트웨어 리셋 */
    rc522_reset();

    /* 내부 타이머 설정 (카드 응답 대기 시간 계산에 사용) */
    rc522_write_reg(REG_T_MODE,      0x8D);
    rc522_write_reg(REG_T_PRESCALER, 0x3E);
    rc522_write_reg(REG_T_RELOAD_L,  30);
    rc522_write_reg(REG_T_RELOAD_H,  0);

    /* 100% ASK 변조 방식 설정 (RFID 송신 신호를 정상적으로 내보내기 위한 설정) */
    rc522_write_reg(REG_TX_ASK, 0x40);

    /* CRC 초기값 설정 (ISO 14443-3) */
    rc522_write_reg(REG_MODE, 0x3D);

    rc522_antenna_on();
}

uint8_t rfid_get_version(void)
{
    return rc522_read_reg(REG_VERSION);
}

/* ═══════════════════════════════════════════
   카드 통신 핵심 루틴
═══════════════════════════════════════════*/

/* RC522와 카드 사이의 실제 통신
   데이터 보내고, 카드 응답 받아서 돌려줌 */
rfid_status_t rc522_to_card(uint8_t cmd,
                             uint8_t *send_data, uint8_t send_len,
                             uint8_t *back_data, uint16_t *back_len)
{
    rfid_status_t status = RFID_ERROR; // 기본 상태: 에러
    uint8_t  irq_en    = 0x00; // 어떤 인터럽트를 허용할지
    uint8_t  wait_irq  = 0x00; // 어떤 완료 비트를 기다릴지
    uint8_t  n;
    uint16_t i;

    // 송수신 명령일 때만 인터럽트 설정
    if (cmd == CMD_TRANSCEIVE) {
        irq_en   = 0x77; /* 인터럽트 활성화 마스크 */
        wait_irq = 0x30;
    }

    rc522_write_reg(REG_COM_IEN, irq_en | 0x80); // 0x80 = IRQ핀 출력 활성화
    rc522_clear_bits(REG_COM_IRQ, 0x80);         // 기존 인터럽트 플래그 초기화
    rc522_set_bits(REG_FIFO_LEVEL, 0x80);        // FIFO 버퍼 비우기 (FlushBuffer)
    rc522_write_reg(REG_COMMAND, CMD_IDLE);      // RC522를 대기 상태로 전환

    /* FIFO에 보낼 데이터 적재: CS 유지한 채 주소 1번 보냄
       -> 그 뒤에 데이터 N번 연속 전송
       (매 바이트마다 주소를 재전송하면 RC522가 오류 처리함) */
    SPI_CS_LOW(_cs);
    spi_transfer_byte((REG_FIFO_DATA << 1) & 0x7E); /* 주소 1회 */
    for (uint8_t k = 0; k < send_len; k++) {
        spi_transfer_byte(send_data[k]); /* 데이터 연속 */
    }
    SPI_CS_HIGH(_cs);

    // 명령 실행 - CMD_TRANSCEIVE면 송신 후 수신까지 수행
    rc522_write_reg(REG_COMMAND, cmd);
    if (cmd == CMD_TRANSCEIVE) {
        rc522_set_bits(REG_BIT_FRAMING, 0x80);
    }

    /* 카드 응답 대기 루프
       최대 2000번 반복하면서 인터럽트 플래그 확인
       타임아웃(i=0) 또는 응답 수신 시 루프 탈출 */
    i = 2000;
    do {
        n = rc522_read_reg(REG_COM_IRQ);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & wait_irq));

    rc522_clear_bits(REG_BIT_FRAMING, 0x80);

    if (i == 0) return RFID_TIMEOUT;
    // 0x1B = 버퍼오버플로우, 패리티, CRC 에러 비트 마스크
    if (rc522_read_reg(REG_ERROR) & 0x1B) return RFID_ERROR;

    // 정상 응답이면 FIFO에서 결과 읽기
    if (n & wait_irq) {
        if (!(n & 0x08)) {
            if (cmd == CMD_TRANSCEIVE) {
                uint8_t last_bits;
                uint8_t fifo_n = rc522_read_reg(REG_FIFO_LEVEL);
                last_bits = rc522_read_reg(REG_CONTROL) & 0x07;

                // 받은 데이터 비트 수 계산 (마지막 바이트가 8비트 미만일 수 있음)
                *back_len = last_bits ? (fifo_n - 1) * 8 + last_bits
                                       : fifo_n * 8;
                if (fifo_n == 0) fifo_n = 1;

                // FIFO에 들어온 응답 데이터를 하나씩 읽어 저장
                for (i = 0; i < fifo_n; i++) {
                    back_data[i] = rc522_read_reg(REG_FIFO_DATA);
                }
            }
            status = RFID_OK;
        }
    }

    return status;
}

/* 근처에 카드가 있는지 확인하는 함수 */
rfid_status_t rc522_request(uint8_t req_mode, uint8_t *tag_type)
{
    rfid_status_t status;
    uint16_t back_bits;

    rc522_write_reg(REG_BIT_FRAMING, 0x07); // 마지막 바이트를 7비트만 전송

    // REQA 명령 1바이트 전송 후 응답 수신
    tag_type[0] = req_mode;
    status = rc522_to_card(CMD_TRANSCEIVE, tag_type, 1, tag_type, &back_bits);

    // 응답이 없거나 16비트(ATQA)가 아니면 카드 없음으로 처리
    if ((status != RFID_OK) || (back_bits != 0x10)) {
        return RFID_NOTAG;
    }
    return RFID_OK;
}

/* 충돌 방지 처리 + UID 읽기 함수
   여러 카드가 동시에 있을 때 하나를 선택 */
rfid_status_t rc522_anticoll(rfid_uid_t *uid)
{
    rfid_status_t status;
    uint8_t  serial_check = 0;
    uint8_t  serial_buf[2];
    uint16_t back_bits;
    uint8_t  back_data[8];

    rc522_write_reg(REG_BIT_FRAMING, 0x00);

    // 안티콜리전 명령 준비 (0x93, 0x20)
    serial_buf[0] = PICC_ANTICOLL;
    serial_buf[1] = 0x20;

    status = rc522_to_card(CMD_TRANSCEIVE,
                            serial_buf, 2,
                            back_data, &back_bits);
    if (status == RFID_OK) {
        // UID 4바이트를 XOR해서 체크섬 검증
        // back_data[4]가 카드가 보낸 체크섬 -> 불일치면 에러
        for (uint8_t i = 0; i < 4; i++) {
            serial_check ^= back_data[i];
        }
        if (serial_check != back_data[4]) return RFID_ERROR;

        // UID 4바이트를 구조체에 저장
        uid->size = 4;
        for (uint8_t i = 0; i < 4; i++) {
            uid->bytes[i] = back_data[i];
        }
    }
    return status;
}

/* 외부에서 호출하는 최종 함수. 카드 감지 + UID 읽기를 한 번에 처리 */
rfid_status_t rfid_read_uid(rfid_uid_t *uid)
{
    uint8_t tag_type[2];

    // 먼저 카드가 근처에 있는지 확인 (REQA 명령을 보내서 응답이 오면 카드 존재)
    if (rc522_request(PICC_REQA, tag_type) != RFID_OK) {
        return RFID_NOTAG;
    }

    // 카드가 있으면 안티콜리전 수행 (여러 카드 충돌 방지 후 UID 읽어옴)
    return rc522_anticoll(uid);
}

/* 전체 동작 흐름
 * rfid_read_uid() 호출
 *   -> rc522_request()  : 카드 여부 (REQA 명령)
 *   -> rc522_anticoll()  : UID 4바이트 읽기 + 체크섬 검증
 *   -> uid 구조체에 4바이트 UID 저장 완료
 */
