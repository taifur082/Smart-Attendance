#include "rc522.h"
#include "sdkconfig.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "RC522";

/* ── Pin config from Kconfig ── */
#define MOSI_PIN   CONFIG_RC522_SPI_MOSI_PIN
#define MISO_PIN   CONFIG_RC522_SPI_MISO_PIN
#define SCK_PIN    CONFIG_RC522_SPI_SCK_PIN
#define CS_PIN     CONFIG_RC522_SPI_CS_PIN
#define RST_PIN    CONFIG_RC522_RST_PIN
#define SPI_CLK_HZ CONFIG_RC522_SPI_CLOCK_HZ

/* ── MFRC522 register addresses ── */
#define REG_COMMAND        0x01
#define REG_COM_IEN        0x02
#define REG_COM_IRQ        0x04
#define REG_ERROR          0x06
#define REG_FIFO_DATA      0x09
#define REG_FIFO_LEVEL     0x0A
#define REG_CONTROL        0x0C
#define REG_BIT_FRAMING    0x0D
#define REG_COLL           0x0E
#define REG_MODE           0x11
#define REG_TX_CONTROL     0x14
#define REG_TX_ASK         0x15
#define REG_CRC_RESULT_H   0x21
#define REG_CRC_RESULT_L   0x22
#define REG_T_MODE         0x2A
#define REG_T_PRESCALER    0x2B
#define REG_T_RELOAD_H     0x2C
#define REG_T_RELOAD_L     0x2D
#define REG_AUTO_TEST      0x36
#define REG_VERSION        0x37

/* ── MFRC522 commands ── */
#define CMD_IDLE           0x00
#define CMD_MEM            0x01
#define CMD_CALC_CRC       0x03
#define CMD_TRANSCEIVE     0x0C
#define CMD_SOFT_RESET     0x0F

/* ── PICC (card) commands ── */
#define PICC_REQA          0x26
#define PICC_ANTICOLL      0x93

/* ── SPI read/write ── */
#define SPI_READ_MASK(reg)  (((reg) << 1) | 0x80)
#define SPI_WRITE_MASK(reg) (((reg) << 1) & 0x7E)

static spi_device_handle_t s_spi_dev = NULL;

/* ------------------------------------------------------------------ */
/* Low-level SPI helpers                                               */
/* ------------------------------------------------------------------ */

static void rc522_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { SPI_WRITE_MASK(reg), val };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
    };
    spi_device_transmit(s_spi_dev, &t);
}

static uint8_t rc522_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { SPI_READ_MASK(reg), 0x00 };
    uint8_t rx[2] = { 0, 0 };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(s_spi_dev, &t);
    return rx[1];
}

static void rc522_set_bit_mask(uint8_t reg, uint8_t mask)
{
    rc522_write_reg(reg, rc522_read_reg(reg) | mask);
}

static void rc522_clear_bit_mask(uint8_t reg, uint8_t mask)
{
    rc522_write_reg(reg, rc522_read_reg(reg) & (~mask));
}

/* ------------------------------------------------------------------ */
/* MFRC522 initialisation                                              */
/* ------------------------------------------------------------------ */

static void rc522_soft_reset(void)
{
    rc522_write_reg(REG_COMMAND, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void rc522_antenna_on(void)
{
    uint8_t val = rc522_read_reg(REG_TX_CONTROL);
    if ((val & 0x03) != 0x03) {
        rc522_set_bit_mask(REG_TX_CONTROL, 0x03);
    }
}

esp_err_t rc522_init(void)
{
    /* GPIO for RST */
    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << RST_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst_cfg);
    gpio_set_level(RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* SPI bus */
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64,
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* SPI device */
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = SPI_CLK_HZ,
        .mode = 0,
        .spics_io_num = CS_PIN,
        .queue_size = 4,
    };
    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Hardware reset via RST pin */
    gpio_set_level(RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    rc522_soft_reset();

    /* Check version register */
    uint8_t ver = rc522_read_reg(REG_VERSION);
    if (ver == 0x00 || ver == 0xFF) {
        ESP_LOGE(TAG, "RC522 not detected (version=0x%02X). Check wiring.", ver);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "RC522 detected, firmware version: 0x%02X", ver);

    /* Timer: 25ms timeout for card communication */
    rc522_write_reg(REG_T_MODE,      0x8D);
    rc522_write_reg(REG_T_PRESCALER, 0x3E);
    rc522_write_reg(REG_T_RELOAD_L,  0x1E);
    rc522_write_reg(REG_T_RELOAD_H,  0x00);

    rc522_write_reg(REG_TX_ASK, 0x40);   /* 100% ASK modulation */
    rc522_write_reg(REG_MODE,   0x3D);   /* CRC preset 0x6363 (ISO14443) */

    rc522_antenna_on();

    ESP_LOGI(TAG, "RC522 initialized: MOSI=%d MISO=%d SCK=%d CS=%d RST=%d CLK=%d Hz",
             MOSI_PIN, MISO_PIN, SCK_PIN, CS_PIN, RST_PIN, SPI_CLK_HZ);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* PICC communication                                                  */
/* ------------------------------------------------------------------ */

/**
 * Transceive data to/from the card.
 * Returns ESP_OK on success; sets *rx_bits to valid bits in last byte.
 */
static esp_err_t rc522_transceive(
    const uint8_t *tx_data, uint8_t tx_len,
    uint8_t *rx_data, uint8_t *rx_len, uint8_t *rx_bits_last)
{
    rc522_write_reg(REG_COM_IRQ,    0x7F);   /* clear IRQ flags */
    rc522_set_bit_mask(REG_FIFO_LEVEL, 0x80); /* flush FIFO */

    rc522_write_reg(REG_COMMAND, CMD_IDLE);

    /* Write data to FIFO */
    for (uint8_t i = 0; i < tx_len; i++) {
        rc522_write_reg(REG_FIFO_DATA, tx_data[i]);
    }

    rc522_write_reg(REG_COMMAND, CMD_TRANSCEIVE);
    rc522_set_bit_mask(REG_BIT_FRAMING, 0x80); /* start transmission */

    /* Wait for RxIRq or timer */
    uint16_t timeout = 2000;
    uint8_t irq;
    do {
        irq = rc522_read_reg(REG_COM_IRQ);
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (!(irq & 0x31) && timeout > 0); /* RxIRq | IdleIRq | TimerIRq */

    rc522_clear_bit_mask(REG_BIT_FRAMING, 0x80); /* stop transmission */

    if (timeout == 0) {
        return ESP_ERR_TIMEOUT;
    }
    if (irq & 0x01) { /* TimerIRq */
        return ESP_ERR_TIMEOUT;
    }

    uint8_t err = rc522_read_reg(REG_ERROR);
    if (err & 0x1B) { /* BufferOvfl | ColErr | ParityErr | ProtocolErr */
        ESP_LOGD(TAG, "transceive error: 0x%02X", err);
        return ESP_FAIL;
    }

    uint8_t n = rc522_read_reg(REG_FIFO_LEVEL);
    uint8_t last_bits = rc522_read_reg(REG_CONTROL) & 0x07;

    if (rx_data && rx_len) {
        if (n > *rx_len) n = *rx_len;
        for (uint8_t i = 0; i < n; i++) {
            rx_data[i] = rc522_read_reg(REG_FIFO_DATA);
        }
        *rx_len = n;
    }
    if (rx_bits_last) {
        *rx_bits_last = last_bits;
    }

    return ESP_OK;
}

/**
 * Send REQA command; returns true if a card acknowledges.
 */
static bool rc522_request(void)
{
    rc522_write_reg(REG_BIT_FRAMING, 0x07); /* 7 bits in last byte */

    uint8_t req = PICC_REQA;
    uint8_t rx[2];
    uint8_t rx_len = sizeof(rx);
    uint8_t last_bits = 0;

    esp_err_t ret = rc522_transceive(&req, 1, rx, &rx_len, &last_bits);
    rc522_write_reg(REG_BIT_FRAMING, 0x00); /* restore */

    if (ret != ESP_OK || rx_len < 1) {
        return false;
    }
    /* ATQA must be 2 bytes with bits set; minimal check: not 0x00 */
    return (rx_len == 2);
}

/**
 * Anti-collision: read UID bytes.
 * Returns ESP_OK and fills uid_bytes/uid_len on success.
 */
static esp_err_t rc522_anticoll(uint8_t *uid_bytes, uint8_t *uid_len)
{
    rc522_write_reg(REG_BIT_FRAMING, 0x00);
    rc522_clear_bit_mask(REG_COLL, 0x80); /* ValuesAfterColl = 0 */

    uint8_t tx[2] = { PICC_ANTICOLL, 0x20 };
    uint8_t rx[5];
    uint8_t rx_len = sizeof(rx);
    uint8_t last_bits = 0;

    esp_err_t ret = rc522_transceive(tx, 2, rx, &rx_len, &last_bits);
    if (ret != ESP_OK || rx_len < 5) {
        return ESP_FAIL;
    }

    /* Simple BCC check: XOR of bytes 0..3 must equal byte 4 */
    uint8_t bcc = 0;
    for (int i = 0; i < 4; i++) bcc ^= rx[i];
    if (bcc != rx[4]) {
        ESP_LOGD(TAG, "BCC mismatch");
        return ESP_FAIL;
    }

    /* UID is the first 4 bytes */
    for (int i = 0; i < 4; i++) uid_bytes[i] = rx[i];
    *uid_len = 4;
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool rc522_detect_uid(uint8_t *uid_bytes, uint8_t *uid_len)
{
    if (!uid_bytes || !uid_len) return false;
    if (!rc522_request()) return false;
    return rc522_anticoll(uid_bytes, uid_len) == ESP_OK;
}

void rc522_uid_to_hex(const uint8_t *uid_bytes, uint8_t uid_len, char *out_str)
{
    for (uint8_t i = 0; i < uid_len; i++) {
        snprintf(out_str + i * 2, 3, "%02X", uid_bytes[i]);
    }
    out_str[uid_len * 2] = '\0';
}
