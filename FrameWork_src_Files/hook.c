#include "hook.h"
#include <string.h>
#include <stdio.h>
#include "Fault_state.h"
#include "FaultInjection_Interface.h"
#include "NvM_ConfigTypes.h" 

#undef NvM_WriteBlock
#undef NvM_ReadBlock
#undef NvM_InvalidateNvBlock
#undef NvM_EraseNvBlock
#undef NvM_SetDataIndex
#undef Fls_Write

extern Std_ReturnType NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr );	
extern Std_ReturnType NvM_WriteBlock( NvM_BlockIdType blockId, const void *NvM_SrcPtr );	
extern Std_ReturnType NvM_InvalidateNvBlock( NvM_BlockIdType blockId );
extern Std_ReturnType NvM_EraseNvBlock( NvM_BlockIdType blockId );
extern Std_ReturnType NvM_SetDataIndex( NvM_BlockIdType blockId, uint8 DataIndex );
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
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;

    /* Local copies for corruption */
    NvM_BlockIdType activeBlockId = blockId;
    const void* activeSrcPtr = NvM_SrcPtr;


    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_NVM, i) == TRUE) {
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
/* --- READ HOOK --- */
Std_ReturnType Hook_NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr ) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;

    /* Local copies for corruption */
    NvM_BlockIdType activeBlockId = blockId;
    void* activeDstPtr = NvM_DstPtr;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_NVM, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             uint32_t dataLength = GetNvMBlockLength(activeBlockId);
             
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
Std_ReturnType Hook_NvM_InvalidateNvBlock( NvM_BlockIdType blockId ) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;
    NvM_BlockIdType activeBlockId = blockId;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_NVM, i) == TRUE) {
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
Std_ReturnType Hook_NvM_EraseNvBlock( NvM_BlockIdType blockId ) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;
    NvM_BlockIdType activeBlockId = blockId;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_NVM, i) == TRUE) {
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
Std_ReturnType Hook_NvM_SetDataIndex( NvM_BlockIdType blockId, uint8 DataIndex ) {
    uint16_t i;
    Std_ReturnType retVal = E_OK;
    boolean callOriginal = TRUE;
    boolean corruptReturnValue = FALSE;
    
    /* Local copies of parameters that we can corrupt */
    NvM_BlockIdType activeBlockId = blockId;
    uint8 activeDataIndex = DataIndex;

    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(FAULT_TARGET_NVM, i) == TRUE) {
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
                     /* 
                      * For synchronous functions, omission means we just 
                      * skip the function call and return an error.
                      * Note: QUEUE_OVERFLOW is removed because it does not apply.
                      */
                     callOriginal = FALSE;
                     retVal = E_NOT_OK;
                     break;
                 }
                 case FAULT_PARAMETER_CORRUPTION:
                 {
                     /* 
                      * Corrupt the inputs so the real NvM_SetDataIndex rejects them 
                      * and fires a DET error (e.g., NVM_E_PARAM_BLOCK_ID or NVM_E_PARAM_BLOCK_DATA_IDX)
                      */
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
