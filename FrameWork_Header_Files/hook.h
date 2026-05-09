#ifndef HOOK_H
#define HOOK_H

#include "Std_Types.h"
#include "NvM_ConfigTypes.h"
#include "FaultInjection_Types.h"
#include "MemIf_Types.h"

/* Global Configuration Instance */
extern FaultConfig_t Global_FaultConfig;

/* Interceptor Prototypes */
Std_ReturnType Hook_NvM_ReadBlock(NvM_BlockIdType blockId, void *NvM_DstPtr);
Std_ReturnType Hook_NvM_WriteBlock(NvM_BlockIdType blockId, const void *NvM_SrcPtr);
Std_ReturnType Hook_NvM_InvalidateNvBlock(NvM_BlockIdType blockId);
Std_ReturnType Hook_NvM_EraseNvBlock(NvM_BlockIdType blockId);
Std_ReturnType Hook_NvM_SetDataIndex(NvM_BlockIdType blockId, uint8 DataIndex);

Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
Std_ReturnType Hook_Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
Std_ReturnType Hook_Fls_Erase(uint32 TargetAddress, uint32 Length);
Std_ReturnType Hook_Fee_Write(uint16 blockNumber, uint8* dataBufferPtr);
Std_ReturnType Hook_Fee_Read(uint16 blockNumber, uint16 blockOffset, uint8* dataBufferPtr, uint16 length);
MemIf_JobResultType Hook_Fee_GetJobResult(void);
MemIf_StatusType Hook_Fee_GetStatus(void);
void PrintFeeRuntime(const char* phase);

/* ALWAYS Redirect calls to the Hook.
 * Fault-type switching happens inside each hook via FaultState_IsActive / switch.
 */
#ifndef NVM_INTERNAL_BUILD
#define NvM_WriteBlock        Hook_NvM_WriteBlock
#define NvM_ReadBlock         Hook_NvM_ReadBlock
#define NvM_InvalidateNvBlock Hook_NvM_InvalidateNvBlock
#define NvM_EraseNvBlock      Hook_NvM_EraseNvBlock
#define NvM_SetDataIndex      Hook_NvM_SetDataIndex
#endif

#define Fls_Write       Hook_Fls_Write
#define Fls_Read        Hook_Fls_Read
#define Fls_Erase       Hook_Fls_Erase

#ifndef FEE_INTERNAL_BUILD
#define Fee_Write        Hook_Fee_Write
#define Fee_Read         Hook_Fee_Read
#define Fee_GetJobResult Hook_Fee_GetJobResult
#define Fee_GetStatus    Hook_Fee_GetStatus
#endif

#endif /* HOOK_H */
