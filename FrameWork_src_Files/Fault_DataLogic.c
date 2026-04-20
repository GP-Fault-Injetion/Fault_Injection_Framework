#include "Fault_DataLogic.h"
#include <stddef.h>

/* Internal State for Xorshift32 */
static uint32 xorshift_state = 0xDEADC0DE; 

void Fault_SeedRandom(uint32 seed) {
    if (seed != 0) {
        xorshift_state = seed;
    }
}

uint32 Fault_GetRandomValue(void) {
    uint32 x = xorshift_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    xorshift_state = x;
    return x;
}

uint32 Fault_GetOutOfRangeValue(uint32 min, uint32 max) {
    /* Case 1: Force Underflow */
    if (min > 0) {
        return (min - 1);
    }
    /* Case 2: Force Overflow */
    if (max < 0xFFFFFFFFU) {
        return (max + 1);
    }
    return max; 
}

/* * Requirement: Burst Error Injection
 * Updated: Returns E_OK on success, E_NOT_OK on failure.
 */
Std_ReturnType Fault_InjectBurstError(uint8* buffer, uint32 len, uint8 burstLen) {
    /* Validation Checks */
    if ((buffer == NULL) || (len == 0) || (burstLen == 0)) {
        return E_NOT_OK;
    }

    /* Safety Cap: If burst is longer than buffer, cap it */
    if (burstLen > len) {
        burstLen = (uint8)len;
    }

    /* Calculate safe boundary */
    uint32 max_start_index = len - burstLen;

    /* Pick a random start index within safe bounds */
    uint32 start_index = Fault_GetRandomValue() % (max_start_index + 1);

    /* Inject random noise */
    for (uint8 i = 0; i < burstLen; i++) {
        buffer[start_index + i] = (uint8)(Fault_GetRandomValue() & 0xFF);
    }

    return E_OK;
}

/* * Requirement: CRC Field Corruption
 * Updated: Returns E_OK on success, E_NOT_OK on failure.
 */
Std_ReturnType Fault_CorruptCRCField(uint8* buffer, uint32 totalLen, uint8 crcWidth) {
    /* Validation Checks */
    if ((buffer == NULL) || (totalLen < crcWidth) || (crcWidth == 0)) {
        return E_NOT_OK;
    }

    /* Point to the start of the CRC field */
    uint32 crc_start_index = totalLen - crcWidth;

    /* Corrupt the CRC bytes */
    for (uint32 i = 0; i < crcWidth; i++) {
        buffer[crc_start_index + i] ^= 0xAA; 
    }

    return E_OK;
}