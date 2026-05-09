#ifndef HOOK_H
#define HOOK_H

#include "Std_Types.h"
#include "NvM.h"
#include "FaultInjection_Types.h"
#include "NvM_ConfigTypes.h"
#include "Fls.h"

/* Global Configuration Instance */
extern FaultConfig_t Global_FaultConfig;

/* Interceptor Prototypes */
Std_ReturnType Hook_NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr );
Std_ReturnType Hook_NvM_WriteBlock( NvM_BlockIdType blockId, const void *NvM_SrcPtr );
Std_ReturnType Hook_NvM_InvalidateNvBlock( NvM_BlockIdType blockId );
Std_ReturnType Hook_NvM_EraseNvBlock( NvM_BlockIdType blockId );
Std_ReturnType Hook_NvM_SetDataIndex( NvM_BlockIdType blockId, uint8 DataIndex );

Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);

/* 
 * ALWAYS Redirect calls to the Hook.
 * The "Switching" will happen inside the Hook function using 'Global_FaultConfig.Active' 
 * or the newer FaultState manager array.
 */
#define NvM_ReadBlock         Hook_NvM_ReadBlock
#define NvM_WriteBlock        Hook_NvM_WriteBlock
#define NvM_InvalidateNvBlock Hook_NvM_InvalidateNvBlock
#define NvM_EraseNvBlock      Hook_NvM_EraseNvBlock
#define NvM_SetDataIndex      Hook_NvM_SetDataIndex

#define Fls_Write             Hook_Fls_Write

#endif /* HOOK_H */