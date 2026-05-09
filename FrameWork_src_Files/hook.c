#include "hook.h"
#include <string.h>
#include <stdio.h>

/* Full includes needed for hook.c implementation */
#include "FaultInjection_Types.h"
#include "NvM.h"
#include "NvM_ConfigTypes.h"
#include "Fls.h"
#include "Fee.h"
#include "Fee_Cfg.h"
#include "Fee_ConfigTypes.h"
#include "Fault_state.h"
#include "FaultInjection_Interface.h"

typedef enum {
    FEE_STATUS_FAULT_BUSY_TO_IDLE,
    FEE_STATUS_FAULT_IDLE_TO_BUSY,
    FEE_STATUS_FAULT_IDLE_TO_UNINIT,
    FEE_STATUS_FAULT_TO_BUSY_INTERNAL
} state_corruption;

/* --- Undef NvM --- */
#undef NvM_WriteBlock
#undef NvM_ReadBlock
#undef NvM_InvalidateNvBlock
#undef NvM_EraseNvBlock
#undef NvM_SetDataIndex
/* --- Undef Fls --- */
#undef Fls_Write
#undef Fls_Read
#undef Fls_Erase
/* --- Undef Fee --- */
#undef Fee_Write
#undef Fee_GetJobResult
#undef Fee_GetStatus
#undef Fee_Read

extern Std_ReturnType NvM_ReadBlock(NvM_BlockIdType blockId, void *NvM_DstPtr);
extern Std_ReturnType NvM_WriteBlock(NvM_BlockIdType blockId, const void *NvM_SrcPtr);
extern Std_ReturnType NvM_InvalidateNvBlock(NvM_BlockIdType blockId);
extern Std_ReturnType NvM_EraseNvBlock(NvM_BlockIdType blockId);
extern Std_ReturnType NvM_SetDataIndex(NvM_BlockIdType blockId, uint8 DataIndex);
extern Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
extern Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
extern Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length);
extern uint8 VirtualFlashMemory[];
extern Std_ReturnType Fee_Write(uint16 blockNumber, uint8* dataBufferPtr);
extern Std_ReturnType Fee_Read(uint16 blockNumber, uint16 blockOffset, uint8* dataBufferPtr, uint16 length);
extern MemIf_StatusType Fee_GetStatus(void);
extern MemIf_JobResultType Fee_GetJobResult(void);


extern const NvM_ConfigType NvM_Config;
extern FaultConfig_t* FaultState_GetConfig(uint16_t fault_Id);

void PrintBuffer2(const char* label, uint8* buf, uint8* reference) {
    printf("%s:\n", label);
    for (int i = 0; i < 66; i++) {
        if (reference != NULL && buf[i] != reference[i]) {
            printf("[0x%02X] ", buf[i]);
        } else {
            printf("0x%02X ", buf[i]);
        }
    }
    printf("\n");
}

/* --- HELPER: NvM Length Lookup --- */
static uint32_t GetNvMBlockLength(NvM_BlockIdType blockId) {
    if (blockId < 1) return 0;
    uint16_t index = blockId - 1;
    return (uint32_t)NvM_Config.BlockDescriptor[index].NvBlockLength;
}

/* --- HELPER: Robust Fee Header Detection --- */
static boolean IsFeeHeader(const uint8* data, uint32 len) {
    /* Fee Headers contain Magic Numbers (0xEBBABABE or 0x2345BABE).
       They are often at Offset 8, but can vary by configuration.
       We scan the first 16 bytes to be sure. */
    if (len < 12) return FALSE;

    uint32_t* words = (uint32_t*)data;
    int i;
    for (i = 0; i < 4 && i < (int)(len / 4); i++) {
        if (words[i] == 0xEBBABABE || words[i] == 0x2345BABE) {
            return TRUE;
        }
    }
    return FALSE;
}

/* --- HELPER: Find first active fault for a given target --- */
static FaultConfig_t* FindActiveFault(uint16_t targetId) {
    uint16_t i;
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(targetId, i) == TRUE) {
            return FaultState_GetConfig(i);
        }
    }
    return NULL;
}

/* =========================================================================
 * NVM HOOKS
 * ========================================================================= */

/* --- NVM WRITE HOOK --- */
Std_ReturnType Hook_NvM_WriteBlock(NvM_BlockIdType blockId, const void *NvM_SrcPtr) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;

    /* Local copies for corruption */
    NvM_BlockIdType activeBlockId = blockId;
    const void* activeSrcPtr = NvM_SrcPtr;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_NVM_WRITE_BLOCK, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);

             switch(cfg->Type) {
                 case FAULT_DELAY:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_OMISSION:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_QUEUE_OVERFLOW:
                 {
                     //just call the original function
                     break;
                 }
                 case FAULT_PARAMETER_CORRUPTION:
                 {
                     /* Corrupt the Block ID so NvM_WriteBlock rejects it */
                     activeBlockId ^= 0xFFFF;
                     /* Corrupt Address Pointer to trigger NVM_E_PARAM_ADDRESS */
                     activeSrcPtr = NULL;
                     break;
                 }
                 case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
                 {
                     /* Corrupt the return value after calling the original function */
                     corruptReturnValue = TRUE;
                     break;
                 }
                 case FAULT_RETURN_VALUE_REJECTION:
                 {
                     /* Do not call the original function */
                     callOriginal = FALSE;
                     break;
                 }
                 /* Data corruption faults (FAULT_DATA_CORRUPTION, FAULT_BIT_FLIP, etc.)
                    should NOT be injected here for WriteBlock. They should be injected
                    at the FLS/FEE layer so that CRC can discover it , if we injected here wrong CRC will
                  be calculated and it becomes the app resposibelity not BSW . */
                 default:
                     break;
             }
        }
    }

    if(callOriginal == TRUE) {
        retVal = NvM_WriteBlock(activeBlockId, activeSrcPtr);

        if(corruptReturnValue == TRUE) {
            retVal = (retVal == E_OK) ? E_NOT_OK : E_OK;
        }
    }
    else
    {
      /* NvM_E_NOT_OK */
      retVal = E_NOT_OK;
    }

    return retVal;
}

/* --- NVM READ HOOK --- */
Std_ReturnType Hook_NvM_ReadBlock(NvM_BlockIdType blockId, void *NvM_DstPtr) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;

    /* Local copies for corruption */
    NvM_BlockIdType activeBlockId = blockId;
    void* activeDstPtr = NvM_DstPtr;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_NVM_READ_BLOCK, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             uint32_t dataLength = GetNvMBlockLength(activeBlockId);
             (void)dataLength;

             switch(cfg->Type) {
                 case FAULT_DELAY:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_OMISSION:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_QUEUE_OVERFLOW:
                 {
                     //just call the original function
                     break;
                 }
                 case FAULT_PARAMETER_CORRUPTION:
                 {
                     /* Corrupt the Block ID so NvM_ReadBlock rejects it */
                     activeBlockId ^= 0xFFFF;
                     /* Corrupt Address Pointer to trigger NVM_E_PARAM_ADDRESS */
                     activeDstPtr = NULL;
                     break;
                 }
                 case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
                 {
                     /* Corrupt the return value after calling the original function */
                     corruptReturnValue = TRUE;
                     break;
                 }
                 case FAULT_RETURN_VALUE_REJECTION:
                 {
                     /* Do not call the original function */
                     callOriginal = FALSE;
                     break;
                 }
                 /* Data corruption faults (FAULT_DATA_CORRUPTION, FAULT_BIT_FLIP, etc.)
                  should NOT be injected here for ReadBlock. They should be injected
                  at the FLS/FEE layer since NVMRead is async func corrupting the buffer *NvM_DstPtr
                  will just be overwritten by correct data*/
                 default:
                     break;
             }
        }
    }

    if(callOriginal == TRUE) {
        retVal = NvM_ReadBlock(activeBlockId, activeDstPtr);
        if(corruptReturnValue == TRUE) {
            retVal = (retVal == E_OK) ? E_NOT_OK : E_OK;
        }
    }
    else
    {
      /* NvM_E_NOT_OK */
      retVal = E_NOT_OK;
    }

    return retVal;
}

/* --- INVALIDATE HOOK --- */
Std_ReturnType Hook_NvM_InvalidateNvBlock(NvM_BlockIdType blockId) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;
    NvM_BlockIdType activeBlockId = blockId;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_NVM_INVALIDATE_NV_BLOCK, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);

             switch(cfg->Type) {
                 case FAULT_DELAY:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_OMISSION:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_QUEUE_OVERFLOW:
                 {
                     //just call the original function
                     break;
                 }
                 case FAULT_PARAMETER_CORRUPTION:
                 {
                     /* Corrupt the Block ID so NvM_InvalidateNvBlock rejects it */
                     activeBlockId ^= 0xFFFF;
                     break;
                 }
                 case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
                 {
                     /* Corrupt the return value after calling the original function */
                     corruptReturnValue = TRUE;
                     break;
                 }
                 case FAULT_RETURN_VALUE_REJECTION:
                 {
                     /* Do not call the original function */
                     callOriginal = FALSE;
                     break;
                 }
                 default:
                     break;
             }
        }
    }

    if(callOriginal == TRUE) {
        retVal = NvM_InvalidateNvBlock(activeBlockId);

        if(corruptReturnValue == TRUE) {
            retVal = (retVal == E_OK) ? E_NOT_OK : E_OK;
        }
    }
    else
    {
      /* NvM_E_NOT_OK */
      retVal = E_NOT_OK;
    }

    return retVal;
}

/* --- ERASE HOOK --- */
Std_ReturnType Hook_NvM_EraseNvBlock(NvM_BlockIdType blockId) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;
    NvM_BlockIdType activeBlockId = blockId;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_NVM_ERASE_NV_BLOCK, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);

             switch(cfg->Type) {
                 case FAULT_DELAY:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_OMISSION:
                 {
                     //to be implemented
                     break;
                 }
                 case FAULT_QUEUE_OVERFLOW:
                 {
                     //just call the original function
                     break;
                 }
                 case FAULT_PARAMETER_CORRUPTION:
                 {
                     /* Corrupt the Block ID so NvM_EraseNvBlock rejects it */
                     activeBlockId ^= 0xFFFF;
                     break;
                 }
                 case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
                 {
                     /* Corrupt the return value after calling the original function */
                     corruptReturnValue = TRUE;
                     break;
                 }
                 case FAULT_RETURN_VALUE_REJECTION:
                 {
                     /* Do not call the original function */
                     callOriginal = FALSE;
                     break;
                 }
                 default:
                     break;
             }
        }
    }

    if(callOriginal == TRUE) {
        retVal = NvM_EraseNvBlock(activeBlockId);

        if(corruptReturnValue == TRUE) {
            retVal = (retVal == E_OK) ? E_NOT_OK : E_OK;
        }
    }
    else
    {
      /* NvM_E_NOT_OK */
      retVal = E_NOT_OK;
    }

    return retVal;
}

/* --- SET DATA INDEX HOOK (Synchronous) --- */
Std_ReturnType Hook_NvM_SetDataIndex(NvM_BlockIdType blockId, uint8 DataIndex) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;

    /* Local copies of parameters that we can corrupt */
    NvM_BlockIdType activeBlockId = blockId;
    uint8 activeDataIndex = DataIndex;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_NVM_SET_DATA_INDEX, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);

             switch(cfg->Type) {
                 case FAULT_DELAY:
                 {
                     /* Simulate Delay using a busy wait loop before execution */
                     volatile uint32_t delayCount = 10000;
                     while(delayCount--) {}
                     break;
                 }
                 case FAULT_OMISSION:
                 {
                     callOriginal = FALSE;
                     retVal = E_NOT_OK;
                     break;
                 }
                 case FAULT_PARAMETER_CORRUPTION:
                 {
                     activeBlockId ^= 0xFFFF;   /* Flip bits to create a garbage Block ID */
                     activeDataIndex = 255;     /* Flip bits to create a garbage Data Index */
                     break;
                 }
                 case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
                 {
                     /* Corrupt the return value after calling the original function */
                     corruptReturnValue = TRUE;
                     break;
                 }
                 case FAULT_RETURN_VALUE_REJECTION:
                 {
                     /* Do not call the original function */
                     callOriginal = FALSE;
                     break;
                 }
                 default:
                     break;
             }
        }
    }

    /* Execute the real BSW function if it wasn't omitted */
    if(callOriginal == TRUE) {
        retVal = NvM_SetDataIndex(activeBlockId, activeDataIndex);

        /* Corrupt the return value as it leaves the BSW boundary */
        if(corruptReturnValue == TRUE) {
            retVal = (retVal == E_OK) ? E_NOT_OK : E_OK;
        }
    }
    else
    {
      /* NvM_E_NOT_OK */
      retVal = E_NOT_OK;
    }

    return retVal;
}

/* =========================================================================
 * FLS HOOKS
 * ========================================================================= */

/* --- FLS WRITE HOOK --- */
Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length) {
    uint8 tempBuffer[512];
    uint32 actualLen = (Length > 512) ? 512 : Length;
    Std_ReturnType result;

    FaultConfig_t* cfg = FindActiveFault(TARGET_FLS_WRITE);

    if (cfg == NULL) {
        return Fls_Write(TargetAddress, SourceAddressPtr, Length);
    }

    switch (cfg->Type) {
        case FAULT_PARAMETER_CORRUPTION:
            printf("[TEST] Corrupted Fls_Write parameter: SourceAddressPtr -> NULL\n");
            return Fls_Write(TargetAddress, NULL_PTR, Length);

        case FAULT_RETURN_VALUE_REJECTION:
            printf("[TEST] Fls_Write return value REJECTED (no real call): -> 0x%02X\n", E_NOT_OK);
            return E_NOT_OK;

        case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
            result = Fls_Write(TargetAddress, SourceAddressPtr, Length);
            printf("[TEST] Corrupted Fls_Write return value: 0x%02X -> ", result);
            result = (result == E_OK) ? E_NOT_OK : E_OK;
            printf("0x%02X\n", result);
            return result;

        default:
            if (SourceAddressPtr != NULL
                && IsFeeHeader(SourceAddressPtr, actualLen) == FALSE
                && Length >= 32) {
                memcpy(tempBuffer, SourceAddressPtr, actualLen);
                Fault_Inject(tempBuffer, actualLen, cfg);
                return Fls_Write(TargetAddress, tempBuffer, actualLen);
            }
            return Fls_Write(TargetAddress, SourceAddressPtr, Length);
    }
}

/* --- FLS READ HOOK --- */
Std_ReturnType Hook_Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length) {
    Std_ReturnType result;

    FaultConfig_t* cfg = FindActiveFault(TARGET_FLS_READ);

    if (cfg == NULL) {
        return Fls_Read(SourceAddress, TargetAddressPtr, Length);
    }

    switch (cfg->Type) {
        case FAULT_PARAMETER_CORRUPTION:
            printf("[TEST] Corrupted Fls_Read parameter: TargetAddressPtr -> NULL\n");
            return Fls_Read(SourceAddress, NULL_PTR, Length);

        case FAULT_RETURN_VALUE_REJECTION:
            printf("[TEST] Fls_Read return value REJECTED (no real call): -> 0x%02X\n", E_NOT_OK);
            return E_NOT_OK;

        case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
            result = Fls_Read(SourceAddress, TargetAddressPtr, Length);
            printf("[TEST] Corrupted Fls_Read return value: 0x%02X -> ", result);
            result = (result == E_OK) ? E_NOT_OK : E_OK;
            printf("0x%02X\n", result);
            return result;

        default:
            result = Fls_Read(SourceAddress, TargetAddressPtr, Length);
            if (result == E_OK
                && TargetAddressPtr != NULL
                && IsFeeHeader(TargetAddressPtr, Length) == FALSE
                && Length >= 32) {
                Fault_Inject(TargetAddressPtr, Length, cfg);
            }
            return result;
    }
}

/* --- FLS ERASE HOOK --- */
Std_ReturnType Hook_Fls_Erase(uint32 TargetAddress, uint32 Length) {
    Std_ReturnType result;

    FaultConfig_t* cfg = FindActiveFault(TARGET_FLS_ERASE);

    if (cfg == NULL) {
        return Fls_Erase(TargetAddress, Length);
    }

    switch (cfg->Type) {
        case FAULT_PARAMETER_CORRUPTION: {
            uint32 unalignedAddr = TargetAddress + 1;
            printf("[TEST] Corrupted Fls_Erase parameter: TargetAddress -> Unaligned (0x%X)\n", TargetAddress);
            return Fls_Erase(unalignedAddr, Length);
        }

        case FAULT_RETURN_VALUE_REJECTION:
            printf("[TEST] Fls_Erase return value REJECTED (no real call): -> 0x%02X\n", E_NOT_OK);
            return E_NOT_OK;

        case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
            result = Fls_Erase(TargetAddress, Length);
            printf("[TEST] Corrupted Fls_Erase return value: 0x%02X -> ", result);
            result = (result == E_OK) ? E_NOT_OK : E_OK;
            printf("0x%02X\n", result);
            return result;

        default:
            /* Erase normally but leave one dirty byte to simulate incomplete erase */
            result = Fls_Erase(TargetAddress, Length);
            VirtualFlashMemory[TargetAddress] = 0x00;
            return result;
    }
}

/* =========================================================================
 * FEE HOOKS
 * ========================================================================= */

static uint32 GetFeeBlockLength(uint16 blockNumber) {
    uint16 i;
    for (i = 0; i < FEE_NUM_OF_BLOCKS; i++) {
        if (Fee_Config.BlockConfig[i].BlockNumber == blockNumber) {
            return (uint32)Fee_Config.BlockConfig[i].BlockSize;
        }
    }
    return 0;
}

void PrintFeeRuntime(const char* phase) {
    printf("[%s] Fee_GetStatus() = %d, Fee_GetJobResult() = %d\n",
        phase, Fee_GetStatus(), Fee_GetJobResult());
}

Std_ReturnType Hook_Fee_Write(uint16 blockNumber, uint8* dataBufferPtr) {
    uint32 dataLength = GetFeeBlockLength(blockNumber);
    uint16 i;

    if ((dataBufferPtr == NULL) || (dataLength == 0)) {
        return Fee_Write(blockNumber, dataBufferPtr);
    }

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FEE_WRITE, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);

            if (cfg->Type == FAULT_BIT_FLIP || cfg->Type == FAULT_MULTI_BIT_FLIP ||
                cfg->Type == FAULT_STUCK_AT_0 || cfg->Type == FAULT_STUCK_AT_1 ||
                cfg->Type == FAULT_DATA_CORRUPTION || cfg->Type == FAULT_CRC_DATA_CORRUPTION) {
                Fault_Inject(dataBufferPtr, dataLength, cfg);
            } else if (cfg->Type == FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION) {
                Std_ReturnType ret = Fee_Write(blockNumber, dataBufferPtr);
                PrintFeeRuntime("status after real Fee_Write call in FEE case");
                return (ret == E_OK) ? E_NOT_OK : E_OK;
            } else if (cfg->Type == FAULT_RETURN_VALUE_REJECTION) {
                return E_NOT_OK;
            } else if (cfg->Type == FAULT_OMISSION) {
                return E_OK;
            }
        }
    }

    return Fee_Write(blockNumber, dataBufferPtr);
}

Std_ReturnType Hook_Fee_Read(uint16 blockNumber, uint16 blockOffset, uint8* dataBufferPtr, uint16 length) {
    uint32 dataLength = GetFeeBlockLength(blockNumber);
    uint16 i;

    if ((dataBufferPtr == NULL) || (dataLength == 0)) {
        return Fee_Read(blockNumber, blockOffset, dataBufferPtr, length);
    }

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FEE_READ, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);

            if (cfg->Type == FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION) {
                Std_ReturnType ret = Fee_Read(blockNumber, blockOffset, dataBufferPtr, length);
                PrintFeeRuntime("status after real Fee_Read call in FEE case");
                return (ret == E_OK) ? E_NOT_OK : E_OK;
            } else if (cfg->Type == FAULT_RETURN_VALUE_REJECTION) {
                return E_NOT_OK;
            } else if (cfg->Type == FAULT_PARAMETER_CORRUPTION) {
                blockNumber = 0xFFFFU;
                return Fee_Read(blockNumber, blockOffset, dataBufferPtr, length);
            } else if (cfg->Type == FAULT_OMISSION) {
                return E_OK;
            }
        }
    }

    return Fee_Read(blockNumber, blockOffset, dataBufferPtr, length);
}

MemIf_JobResultType Hook_Fee_GetJobResult(void) {
    uint16 i;
    MemIf_JobResultType returnedResult;

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FEE_GET_JOB_RESULT, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);

            if (cfg->Type == FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION) {
                returnedResult = Fee_GetJobResult();
                if (returnedResult == MEMIF_JOB_PENDING) {
                    return MEMIF_JOB_OK;
                }
                return returnedResult;
            }
        }
    }

    return Fee_GetJobResult();
}

MemIf_StatusType Hook_Fee_GetStatus(void) {
    uint16 i;
    MemIf_StatusType realStatus = Fee_GetStatus();

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FEE_GET_STATUS, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);

            if ((cfg != NULL) && (cfg->Type == FAULT_MODULE_STATE_CORRUPTION)) {
                if (realStatus == MEMIF_BUSY) {
                    return MEMIF_IDLE;
                } else if (realStatus == MEMIF_IDLE) {
                    return MEMIF_BUSY;
                }
            }
        }
    }

    return realStatus;
}
