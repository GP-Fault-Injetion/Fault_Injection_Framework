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
Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
Std_ReturnType Hook_Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
Std_ReturnType Hook_Fls_Erase(uint32 TargetAddress, uint32 Length);

/* * ALWAYS Redirect calls to the Hook.
 * The "Switching" will happen inside the Hook function using 'Global_FaultConfig.Active'.
 
 */
#define NvM_WriteBlock  Hook_NvM_WriteBlock
#define NvM_ReadBlock   Hook_NvM_ReadBlock

#define Fls_Write       Hook_Fls_Write
#define Fls_Read        Hook_Fls_Read
#define Fls_Erase       Hook_Fls_Erase

#endif /* HOOK_H */