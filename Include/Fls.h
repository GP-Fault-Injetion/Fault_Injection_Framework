#ifndef FLS_H
#define FLS_H

#include "Std_Types.h"
#include "MemIf_Types.h"

typedef uint32 Fls_AddressType;
typedef uint32 Fls_LengthType;

#define FLS_ERASE_VALUE  0xFF
#define FLS_SECTOR_SIZE  1024

extern volatile boolean FlsJobReady;

/* Standard Job Results */
typedef enum {
    FLS_JOB_OK,
    FLS_JOB_FAILED,
    FLS_JOB_PENDING,
    FLS_JOB_CANCELED
} Fls_JobResultType;

/* API Prototypes */
void Fls_Init(const void* ConfigPtr);
Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length);
void Fls_MainFunction(void);
MemIf_JobResultType Fls_GetJobResult(void);

MemIf_StatusType Fls_GetStatus(void);
void Fls_SetMode(MemIf_ModeType Mode);

#endif /* FLS_H */