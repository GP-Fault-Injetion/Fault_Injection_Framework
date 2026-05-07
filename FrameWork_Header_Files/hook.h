#ifndef HOOK_H
#define HOOK_H

#include "Std_Types.h"
#include "NvM.h"
#include "FaultInjection_Types.h"
#include "NvM_ConfigTypes.h"
#include "Fls.h"

/* Global Configuration Instance */
extern FaultConfig_t Global_FaultConfig;

/* Externs for internal NvM variables to enable testing macros */
extern NvmStateType nvmState;
extern AdministrativeBlockType AdminBlock[];
extern const NvM_ConfigType NvM_Config;

/* Checks if the global NvM state machine has passed the INIT phase */
#define NVM_IS_INITIALIZED() \
    (nvmState != NVM_UNINITIALIZED)

/* Checks the internal job queue or RAM block status to see if a job is active */
#define NVM_IS_BLOCK_PENDING(BlockId) \
    (AdminBlock[(BlockId)-1].ErrorStatus == NVM_REQ_PENDING)

/* Checks the Administrative Block flags for the Write Protection bit */
#define NVM_IS_WRITE_PROTECTED(BlockId) \
    (AdminBlock[(BlockId)-1].BlockWriteProtected == TRUE)

/* Checks the Administrative Block flags for the Locked bit (using our custom bit 0 of flags) */
#define NVM_IS_BLOCK_LOCKED(BlockId) \
    ((AdminBlock[(BlockId)-1].flags & 0x01) != 0u)

/* Checks if a Dataset block's current index points to a ROM block instead of NVRAM */
#define NVM_IS_DATASET_ROM_BLOCK(BlockId) \
    ((NvM_Config.BlockDescriptor[(BlockId)-1].BlockManagementType == NVM_BLOCK_DATASET) && \
     (AdminBlock[(BlockId)-1].DataIndex >= NvM_Config.BlockDescriptor[(BlockId)-1].NvBlockNum))


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