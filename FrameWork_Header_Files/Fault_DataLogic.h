#ifndef FAULT_DATALOGIC_H
#define FAULT_DATALOGIC_H

#include "Std_Types.h" 

/* =============================================================
 * 1. Initialization 
 * ============================================================= */
void Fault_SeedRandom(uint32 seed);

/* =============================================================
 * 2. Value Generation 
 * ============================================================= */
uint32 Fault_GetRandomValue(void);
uint32 Fault_GetOutOfRangeValue(uint32 min, uint32 max);

/* =============================================================
 * 3. Memory Corruption (Updated to return Status)
 * ============================================================= */
Std_ReturnType Fault_InjectBurstError(uint8* buffer, uint32 len, uint8 burstLen);
Std_ReturnType Fault_CorruptCRCField(uint8* buffer, uint32 totalLen, uint8 crcWidth);

#endif /* FAULT_DATALOGIC_H */