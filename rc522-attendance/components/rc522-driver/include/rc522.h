#ifndef RC522_H
#define RC522_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC522_UID_MAX_BYTES 10

/**
 * @brief Initialize the RC522 over SPI.
 *        SPI bus and device are configured from sdkconfig values (Kconfig).
 *        Call once at startup before rc522_detect_uid().
 *
 * @return ESP_OK on success, or an ESP error code.
 */
esp_err_t rc522_init(void);

/**
 * @brief Detect a MIFARE card and read its UID.
 *
 * @param uid_bytes  Output buffer; caller must provide at least RC522_UID_MAX_BYTES bytes.
 * @param uid_len    Output: number of bytes written into uid_bytes (typically 4 or 7).
 * @return true  if a card was detected and its UID was read successfully.
 * @return false if no card is present or communication failed.
 */
bool rc522_detect_uid(uint8_t *uid_bytes, uint8_t *uid_len);

/**
 * @brief Format UID bytes as an uppercase hex string (no separators).
 *        Example: {0xA1, 0xB2, 0xC3, 0xD4} → "A1B2C3D4"
 *
 * @param uid_bytes  UID byte array.
 * @param uid_len    Number of bytes in uid_bytes.
 * @param out_str    Output buffer; must be at least (uid_len * 2 + 1) bytes.
 */
void rc522_uid_to_hex(const uint8_t *uid_bytes, uint8_t uid_len, char *out_str);

#ifdef __cplusplus
}
#endif

#endif // RC522_H
