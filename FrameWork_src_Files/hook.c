#include "hook.h"
#include <string.h>
#include <stdio.h>
#include "Fault_state.h"
#include "FaultInjection_Interface.h"
#include "NvM_ConfigTypes.h" 

#undef NvM_WriteBlock
#undef NvM_ReadBlock
#undef Fls_Write

extern Std_ReturnType NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr );	
extern Std_ReturnType NvM_WriteBlock( NvM_BlockIdType blockId, const void *NvM_SrcPtr );	
extern Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);

extern const NvM_ConfigType NvM_Config;
extern FaultConfig_t* FaultState_GetConfig(uint16_t fault_Id);

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
    
    if (len < 12) return FALSE; /* Too small to be a standard header */

    uint32_t* words = (uint32_t*)data;
    int i;
    
    /* Check first 4 words (16 bytes) for Magic Signature */
    for(i = 0; i < 4 && i < (len/4); i++) {
        if (words[i] == 0xEBBABABE || words[i] == 0x2345BABE) {
            return TRUE; /* Found Magic Number -> It is a Header */
        }
    }
    
    return FALSE; /* No Magic Number -> Likely User Data */
}

/* --- WRITE HOOK --- */
Std_ReturnType Hook_NvM_WriteBlock( NvM_BlockIdType blockId, const void *NvM_SrcPtr ) {
    uint16_t i;
    uint8* mutableDataPtr = (uint8*)NvM_SrcPtr; 
    
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_NVM, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             uint32_t dataLength = GetNvMBlockLength(blockId);
             if(dataLength > 0) {
                 Fault_Inject(mutableDataPtr, dataLength, cfg);
             }
        }
    }
    return NvM_WriteBlock(blockId, NvM_SrcPtr);
}

/* --- READ HOOK --- */
Std_ReturnType Hook_NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr ) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;
    NvM_BlockIdType activeBlockId = blockId;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_NVM, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             uint32_t dataLength = GetNvMBlockLength(activeBlockId);
             
             switch(cfg->Type) {
                 case FAULT_DELAY:
                 {
                     /* Simulate Delay using a busy wait loop */
                     volatile uint32_t delayCount = 10000; /* Arbitrary dummy value */
                     while(delayCount--) {}
                     break;
                 }
                 case FAULT_OMISSION:
                 case FAULT_QUEUE_OVERFLOW:
                 {
                     /* Return without executing */
                     callOriginal = FALSE;
                     retVal = E_NOT_OK;
                     break;
                 }
                 case FAULT_PARAMETER_CORRUPTION:
                 {
                     /* Corrupt the Block ID so NvM_ReadBlock rejects it */
                     activeBlockId ^= 0xFFFF;
                     break;
                 }
                 case FAULT_RETURN_VALUE_CORRUPTION:
                 {
                     corruptReturnValue = TRUE;
                     break;
                 }
                 case FAULT_DATA_CORRUPTION:
                 case FAULT_CRC_DATA_CORRUPTION:
                 case FAULT_BIT_FLIP:
                 case FAULT_MULTI_BIT_FLIP:
                 case FAULT_STUCK_AT_0:
                 case FAULT_STUCK_AT_1:
                 {
                     /* For structural consistency, we inject here. 
                        Note: For asynchronous reads, actual corruption 
                        should ideally happen after data is placed in DstPtr. */
                     if(dataLength > 0 && NvM_DstPtr != NULL) {
                         Fault_Inject((uint8*)NvM_DstPtr, dataLength, cfg);
                     }
                     break;
                 }
                 default:
                     break;
             }
        }
    }

    if(callOriginal == TRUE) {
        retVal = NvM_ReadBlock(activeBlockId, NvM_DstPtr);
        if(corruptReturnValue == TRUE) {
            retVal = (retVal == E_OK) ? E_NOT_OK : E_OK;
        }
    }

    return retVal;
}

/* --- FLS WRITE HOOK IMPLEMENTATION --- */
Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length) {
    
    uint8 tempBuffer[512]; 
    uint32 actualLen = (Length > 512) ? 512 : Length;

    memcpy(tempBuffer, SourceAddressPtr, actualLen);

    uint16_t i;
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_FLS, i) == TRUE) {
             
             /* SMART FILTERING: Check for Magic Number */
             if (IsFeeHeader(SourceAddressPtr, actualLen) == FALSE) {
                 
                 /* Double Check: Only inject if size looks like Data (>= 32) */
                 if (Length >= 32) {
                     FaultConfig_t* cfg = FaultState_GetConfig(i);
                     // printf("[Hook] Injecting FLS Fault on Data (Len=%d)\n", Length);
                     Fault_Inject(tempBuffer, actualLen, cfg);
                 }
             } else {
                 // printf("[Hook] Fee Header Detected. Skipping.\n");
             }
        }
    }

    return Fls_Write(TargetAddress, tempBuffer, actualLen);
}