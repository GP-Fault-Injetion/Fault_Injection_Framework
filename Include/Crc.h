/* Minimal CRC header for test environment */
#ifndef CRC_H
#define CRC_H

#include "Std_Types.h"

uint16 Crc_CalculateCRC16(const uint8* data, uint32 length, uint16 initValue, boolean IsFirstCall);
uint32 Crc_CalculateCRC32(const void *data, uint16 length, uint32 initValue , boolean IsFirstCall);

#endif /* CRC_H */
