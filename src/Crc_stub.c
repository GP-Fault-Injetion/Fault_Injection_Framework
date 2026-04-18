#include "Crc.h"
#include <stdint.h>
#include <stddef.h>

/* * CRC-16 CCITT (Poly: 0x1021)
 * Used by standard AUTOSAR NvM
 */
/* Adjusted for AUTOSAR signature: 4 arguments */
uint16 Crc_CalculateCRC16(const uint8* data, uint32 length, uint16 initValue, boolean IsFirstCall) {
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc;
    uint16_t i;

    /* If it's the first call, override initValue with the standard CCITT start value */
    if (IsFirstCall == TRUE) {
        crc = 0xFFFF; 
    } else {
        crc = initValue;
    }

    while (length--) {
        crc ^= ((uint16_t)(*p++) << 8);
        for (i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

/* * CRC-32 IEEE 802.3 (Poly: 0x04C11DB7)
 * Optional: Only needed if you configure NVM_CRC32
 */
uint32 Crc_CalculateCRC32(const void *data, uint16 length, uint32 initValue , boolean IsFirstCall) {
    
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = initValue ^ 0xFFFFFFFF; // Standard init invert
    uint32_t i;

    if (IsFirstCall == TRUE) {
        crc = 0xFFFFFFFF;
    } else {
        crc = initValue ^ 0xFFFFFFFF; // XOR back because your loop expects inverted input
    }

    while (length--) {
        crc ^= ((uint32_t)(*p++) << 24);
        for (i = 0; i < 8; i++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ 0x04C11DB7;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc ^ 0xFFFFFFFF; // Standard final invert
}