#include "FaultBitLogic.h"
#include "Platform_Types.h"
#include <stddef.h>

/**
 * @brief Single Bit Flip Logic
 * Supports 8, 16, 32-bit types on Little & Big Endian.
 * BitPosition 0 is ALWAYS the Least Significant Bit (Value 1).
 */
void ApplySingleBitFlip(uint8_t* data, uint32_t length, uint8_t bitPosition) {
    uint32_t byteIndex;
    uint8_t  bitOffset = bitPosition % 8;
    uint32_t rawByteIndex = bitPosition / 8;

    if (rawByteIndex < length) {
        #if (CPU_BYTE_ORDER == LOW_BYTE_FIRST)
            /* LITTLE ENDIAN: Index 0 is LSB */
            byteIndex = rawByteIndex;
        #else
            /* BIG ENDIAN: Index (Length-1) is LSB */
            byteIndex = (length - 1u) - rawByteIndex;
        #endif

        data[byteIndex] ^= (1u << bitOffset);
    }
}

/**
 * @brief Single Bit Stuck-At Logic
 * Forces a specific bit to 0 or 1.
 * * @param stuckValue: 1 for Stuck-At-1, 0 for Stuck-At-0
 */
void ApplySingleBitStuckAt(uint8_t* data, uint32_t length, uint8_t bitPosition, uint8_t stuckValue) {
    uint32_t byteIndex;
    uint8_t  bitOffset = bitPosition % 8;
    uint32_t rawByteIndex = bitPosition / 8;

    if (rawByteIndex < length) {
        #if (CPU_BYTE_ORDER == LOW_BYTE_FIRST)
            /* LITTLE ENDIAN: Index 0 is LSB */
            byteIndex = rawByteIndex;
        #else
            /* BIG ENDIAN: Index (Length-1) is LSB */
            byteIndex = (length - 1u) - rawByteIndex;
        #endif

        if (stuckValue == 1) {
            /* Force bit to 1 -> OR with (1 << offset) */
            data[byteIndex] |= (1u << bitOffset);
        } else {
            /* Force bit to 0 -> AND with NOT (1 << offset) */
            data[byteIndex] &= ~(1u << bitOffset);
        }
    }
}

/**
 * @brief Multi-Bit Mask Logic
 * Maps Mask's LSB to Data's LSB, regardless of data length.
 */
void ApplyMultiBitMask(uint8_t* data, uint32_t length, uint32_t mask) {
    uint32_t i;
    uint8_t maskByte;
    uint32_t byteIndex;

    /* Loop through up to 4 bytes of the Mask (32-bit limit) */
    for (i = 0; i < 4u; i++) {
        
        /* Stop if we exceed the data length */
        if (i >= length) {
            break;
        }

        /* Extract the i-th byte from the Mask (starting from LSB) */
        maskByte = (uint8_t)((mask >> (i * 8u)) & 0xFFu);

        /* Skip if mask byte is 0 */
        if (maskByte != 0x00u) {

            #if (CPU_BYTE_ORDER == LOW_BYTE_FIRST)
                /* LITTLE ENDIAN */
                byteIndex = i;
            #else
                /* BIG ENDIAN */
                byteIndex = (length - 1u) - i;
            #endif

            /* Apply the XOR */
            data[byteIndex] ^= maskByte;
        }
    }
}

/**
 * @brief Main Handler
 */
Std_ReturnType ApplyBitFault(uint8_t* data, uint32_t length, const FaultConfig_t* config) {
    Std_ReturnType retVal = E_NOT_OK;

    if ((data != NULL) && (config != NULL)) {
        switch (config->Type) {
            case FAULT_BIT_FLIP:
                ApplySingleBitFlip(data, length, config->BitPosition);
                retVal = E_OK;
                break;

            case FAULT_MULTI_BIT_FLIP:
                ApplyMultiBitMask(data, length, config->Mask);
                retVal = E_OK;
                break;

            case FAULT_STUCK_AT_0:
                /* Force bit defined by BitPosition to 0 */
                ApplySingleBitStuckAt(data, length, config->BitPosition, 0);
                retVal = E_OK;
                break;

            case FAULT_STUCK_AT_1:
                /* Force bit defined by BitPosition to 1 */
                ApplySingleBitStuckAt(data, length, config->BitPosition, 1);
                retVal = E_OK;
                break;

            default:
                /* Other fault types are ignored here */
                retVal = E_NOT_OK;
                break;
        }
    }
    return retVal;
}