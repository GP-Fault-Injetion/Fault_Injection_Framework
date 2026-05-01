#ifndef HOOK_H
#define HOOK_H

#include "Std_Types.h"
#include "NvM_ConfigTypes.h"
#include "FaultInjection_Types.h"

/* Global Configuration Instance */
extern FaultConfig_t Global_FaultConfig;

/* Interceptor Prototypes */
Std_ReturnType Hook_NvM_ReadBlock(NvM_BlockIdType blockId, void *NvM_DstPtr);
Std_ReturnType Hook_NvM_WriteBlock(NvM_BlockIdType blockId, const void *NvM_SrcPtr);
Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
Std_ReturnType Hook_Fee_Write(uint16 blockNumber, uint8* dataBufferPtr);


#ifndef NVM_INTERNAL_BUILD
#define NvM_WriteBlock Hook_NvM_WriteBlock
#define NvM_ReadBlock  Hook_NvM_ReadBlock
#endif

#define Fls_Write      Hook_Fls_Write

#ifndef FEE_INTERNAL_BUILD
#define Fee_Write      Hook_Fee_Write

#endif


#endif /* HOOK_H */