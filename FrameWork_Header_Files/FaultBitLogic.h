#ifndef FAULT_BIT_LOGIC_H
#define FAULT_BIT_LOGIC_H

#include "FaultInjection_Types.h"

/**
 * @file FaultBitLogic.h
 * @brief Implementation of FR-01.1.1: Bit-Level Fault Injection.
 * @author Marina Magdy
 */

/**
 * @brief Main Bit Logic Handler.
 *
 * Routes the request to Single-Bit or Multi-Bit logic based on the fault type.
 *
 * @param[in,out] data    Pointer to the payload.
 * @param[in]     length  Length of the payload in bytes.
 * @param[in]     config  Fault configuration (uses BitPosition or Mask).
 * @return Std_ReturnType E_OK if a bit fault was successfully applied.
 */
Std_ReturnType ApplyBitFault(uint8_t* data, uint32_t length, const FaultConfig_t* config);

/**
 * @brief Helper: Injects a single-bit flip.
 *
 * Supports selecting any bit position (LSB, MSB) regardless of data type.
 *
 * @param[in,out] data         Pointer to data.
 * @param[in]     length       Data length.
 * @param[in]     bitPosition  Absolute bit index to flip (0 = LSB of first byte).
 */
void ApplySingleBitFlip(uint8_t* data, uint32_t length, uint8_t bitPosition);

/**
 * @brief Helper: Injects multi-bit corruption using a mask.
 *
 * Uses the 'Mask' field from configuration to XOR multiple bits at once.
 *
 * @param[in,out] data    Pointer to data.
 * @param[in]     length  Data length.
 * @param[in]     mask    32-bit mask defining which bits to flip.
 */
void ApplyMultiBitMask(uint8_t* data, uint32_t length, uint32_t mask);

#endif /* FAULT_BIT_LOGIC_H */