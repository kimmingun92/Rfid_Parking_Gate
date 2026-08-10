/**
 * my_spi.c
 * AVR ATmega328P 범용 Hardware SPI 드라이버 구현
 */

#include "my_spi.h"

/* ─────────────────────────────────────────
   내부 상태: 현재 설정 보관
──────────────────────────────────────────*/
static spi_config_t _cur_cfg = SPI_CONFIG_DEFAULT;

/* ─────────────────────────────────────────
   내부 헬퍼: 전달받은 설정을 바탕으로
   SPCR / SPSR 레지스터 값을 설정
──────────────────────────────────────────*/
static void _apply_config(const spi_config_t *cfg)
{
    /* SPI 하드웨어 활성, Master 모드 */
    uint8_t spcr = (1 << SPE) | (1 << MSTR);
    uint8_t spsr = 0; // SPI2X 비트 사용 시 on

    /* SPI 모드 설정 (CPOL / CPHA)
       MODE1, MODE3 은 CPHA(위상) = 1
       MODE2, MODE3 은 CPOL(극성) = 1 */
    if (cfg->mode == SPI_MODE1 || cfg->mode == SPI_MODE3) {
        spcr |= (1 << CPHA);
    }
    if (cfg->mode == SPI_MODE2 || cfg->mode == SPI_MODE3) {
        spcr |= (1 << CPOL);
    }

    /* 비트 순서 설정
       DORD: 0 -> MSB First, 1 -> LSB First */
    if (cfg->bit_order == SPI_LSB_FIRST) {
        spcr |= (1 << DORD);
    }

    /* SPI 클럭 분주 설정
       SPR1, SPR0 (SPCR) + SPI2X (SPSR) 3개 비트 조합으로 결정 */
    switch (cfg->clk_div) {
        case SPI_CLK_DIV4:   /* SPR1=0 SPR0=0 SPI2X=0 */
            break;
        case SPI_CLK_DIV16:  /* SPR1=0 SPR0=1 SPI2X=0 */
            spcr |= (1 << SPR0);
            break;
        case SPI_CLK_DIV64:  /* SPR1=1 SPR0=0 SPI2X=0 */
            spcr |= (1 << SPR1);
            break;
        case SPI_CLK_DIV128: /* SPR1=1 SPR0=1 SPI2X=0 */
            spcr |= (1 << SPR1) | (1 << SPR0);
            break;
        case SPI_CLK_DIV2:   /* SPR1=0 SPR0=0 SPI2X=1 */
            spsr |= (1 << SPI2X);
            break;
        case SPI_CLK_DIV8:   /* SPR1=0 SPR0=1 SPI2X=1 */
            spcr |= (1 << SPR0);
            spsr |= (1 << SPI2X);
            break;
        case SPI_CLK_DIV32:  /* SPR1=1 SPR0=0 SPI2X=1 */
            spcr |= (1 << SPR1);
            spsr |= (1 << SPI2X);
            break;
        default:
            break;
    }

    SPCR = spcr;
    SPSR = spsr;
}

/* ═══════════════════════════════════════════
   초기화 / 설정
═══════════════════════════════════════════*/

void spi_init(const spi_config_t *cfg)
{
    /* SCK(PB5), MOSI(PB3) 출력 / MISO(PB4) 입력 */
    DDRB |=  (1 << PB5) | (1 << PB3);
    DDRB &= ~(1 << PB4);

    /* 하드웨어 SS(PB2) 도 출력으로 설정해야 마스터 모드 유지
       (실제 CS 는 spi_cs_t 로 제어하지만, 이 비트가 입력이면
        외부 신호로 마스터→슬레이브 전환 가능 — AVR 주의사항) */
    DDRB  |= (1 << PB2);
    PORTB |= (1 << PB2); /* HIGH: 비선택 상태 */

    if (cfg != NULL) {
        _cur_cfg = *cfg; // 예: MODE0, DIV4, MSB_FIRST
    }
    _apply_config(&_cur_cfg);
}

void spi_reconfigure(const spi_config_t *cfg)
{
    _cur_cfg = *cfg;
    _apply_config(&_cur_cfg);
}

void spi_get_config(spi_config_t *cfg)
{
    *cfg = _cur_cfg;
}

/* ═══════════════════════════════════════════
   CS 핀 관리
   전달 받은 CS핀을 출력으로 만들고 HIGH로 설정
═══════════════════════════════════════════*/
void spi_cs_init(spi_cs_t cs)
{
    *cs.ddr  |= (1 << cs.pin); /* 출력 */
    *cs.port |= (1 << cs.pin); /* HIGH: 비선택 */
}

/* ═══════════════════════════════════════════
   단일 바이트 송수신
   SPI로 1바이트 전송하면서 동시에 1바이트 수신
═══════════════════════════════════════════*/
uint8_t spi_transfer_byte(uint8_t data)
{
    SPDR = data; // 값을 쓰면 SPI 전송 시작
    while (!(SPSR & (1 << SPIF)));
    // SPIF 비트가 1이 될 때까지 대기 (전송 완료 시 하드웨어가 자동 세팅)
    return SPDR; // 동시에 받은 데이터 반환
}

/* ═══════════════════════════════════════════
   버퍼 송수신
═══════════════════════════════════════════*/
void spi_transfer_buf(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t tx = (tx_buf != NULL) ? tx_buf[i] : 0xFF;
        uint8_t rx = spi_transfer_byte(tx);
        if (rx_buf != NULL) {
            rx_buf[i] = rx;
        }
    }
}

void spi_write_buf(const uint8_t *tx_buf, size_t len)
{
    spi_transfer_buf(tx_buf, NULL, len);
}

void spi_read_buf(uint8_t *rx_buf, size_t len)
{
    spi_transfer_buf(NULL, rx_buf, len);
}

/* ═══════════════════════════════════════════
   원자적 트랜잭션 (CS 포함)
   CS LOW -> 데이터 송수신 -> CS HIGH
═══════════════════════════════════════════*/
void spi_transaction(spi_cs_t cs,
                      const uint8_t *tx_buf, uint8_t *rx_buf,
                      size_t len)
{
    SPI_CS_LOW(cs);
    spi_transfer_buf(tx_buf, rx_buf, len);
    SPI_CS_HIGH(cs);
}

/* ═══════════════════════════════════════════
   레지스터 단위 접근 (범용)
═══════════════════════════════════════════*/

/* [주소][데이터] 2바이트를 연속 전송 */
void spi_write_reg(spi_cs_t cs, uint8_t addr, uint8_t data)
{
    SPI_CS_LOW(cs);
    spi_transfer_byte(addr);
    spi_transfer_byte(data);
    SPI_CS_HIGH(cs);
}

/* [주소] 보내고 [데이터] 1바이트 읽기 */
uint8_t spi_read_reg(spi_cs_t cs, uint8_t addr)
{
    uint8_t val;
    SPI_CS_LOW(cs);
    spi_transfer_byte(addr);
    val = spi_transfer_byte(0xFF); // 더미 바이트를 보내면서 데이터 수신
    SPI_CS_HIGH(cs);
    return val;
}

/* [시작주소] 보낸 뒤 len바이트 연속 읽기 */
void spi_read_reg_burst(spi_cs_t cs, uint8_t addr,
                         uint8_t *rx_buf, size_t len)
{
    SPI_CS_LOW(cs);
    spi_transfer_byte(addr);
    spi_read_buf(rx_buf, len);
    SPI_CS_HIGH(cs);
}

/* ═══════════════════════════════════════════
   SPI 전원 제어
═══════════════════════════════════════════*/
void spi_disable(void)
{
    SPCR &= ~(1 << SPE);
}

void spi_enable(void)
{
    SPCR |= (1 << SPE);
}
