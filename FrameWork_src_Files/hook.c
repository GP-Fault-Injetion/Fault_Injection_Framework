#include "hook.h"
#include <string.h>
#include <stdio.h>
#include "Fault_state.h"
#include "FaultInjection_Interface.h"
#include "NvM_ConfigTypes.h" 

#undef NvM_WriteBlock
#undef NvM_ReadBlock
#undef Fls_Write
#undef Fls_Read
#undef Fls_Erase

extern Std_ReturnType NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr );	
extern Std_ReturnType NvM_WriteBlock( NvM_BlockIdType blockId, const void *NvM_SrcPtr );	
extern Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
extern Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
extern Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length);
extern uint8 VirtualFlashMemory[];

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
        if(FaultState_IsActive(TARGET_NVM_WRITE_BLOCK, i) == TRUE) {
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
    return NvM_ReadBlock(blockId, NvM_DstPtr);
}

/* --- FLS WRITE HOOK IMPLEMENTATION --- */
Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length) {
    printf("Fls_Write_hook eeeeeeeeeeeeeeeeee\n");
    uint8 tempBuffer[512]; 
    uint32 actualLen = (Length > 512) ? 512 : Length;
    uint16_t i;
    Std_ReturnType result;

    /* --- CHECK FOR PARAMETER CORRUPTION FIRST --- */
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_FLS_WRITE, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             if (cfg->Type == FAULT_PARAMETER_CORRUPTION) {
                 printf("[TEST] Corrupted Fls_Write parameter: SourceAddressPtr -> NULL\n");
                 /* Call with NULL pointer - FLS should reject it */
                 return Fls_Write(TargetAddress, NULL_PTR, Length);
             }
        }
    }

    /* --- NO PARAMETER CORRUPTION - Check for data corruption --- */
    if (SourceAddressPtr != NULL) {
        memcpy(tempBuffer, SourceAddressPtr, actualLen);
        
        boolean hasDataFault = FALSE;
        for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
            if(FaultState_IsActive(TARGET_FLS_WRITE, i) == TRUE) {
                 FaultConfig_t* cfg = FaultState_GetConfig(i);
                 if (cfg->Type != FAULT_PARAMETER_CORRUPTION && cfg->Type != FAULT_RETURN_VALUE_CORRUPTION) {
                     /* SMART FILTERING: Check for Magic Number */
                     if (IsFeeHeader(SourceAddressPtr, actualLen) == FALSE) {
                         /* Double Check: Only inject if size looks like Data (>= 32) */
                         if (Length >= 32) {
                             Fault_Inject(tempBuffer, actualLen, cfg);
                             hasDataFault = TRUE;
                         }
                     }
                 }
            }
        }
        
        /* Call with original or modified buffer */
        if (hasDataFault) {
            result = Fls_Write(TargetAddress, tempBuffer, actualLen);
        } else {
            result = Fls_Write(TargetAddress, SourceAddressPtr, Length);
        }
    } else {
        /* SourceAddressPtr is NULL - call real function to validate */
        result = Fls_Write(TargetAddress, SourceAddressPtr, Length);
    }

    /* --- CHECK FOR RETURN VALUE CORRUPTION --- */
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_FLS_WRITE, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             if (cfg->Type == FAULT_RETURN_VALUE_CORRUPTION) {
                 /* Corrupt the return value by flipping it */
                 printf("[TEST] Corrupted Fls_Write return value: 0x%02X -> ", result);
                 result = (result == E_OK) ? E_NOT_OK : E_OK;
                 printf("0x%02X\n", result);
             }
        }
    }

    return result;
}

/* --- FLS READ HOOK IMPLEMENTATION --- */
Std_ReturnType Hook_Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length) {
    
    /* --- CHECK FOR PARAMETER CORRUPTION FIRST --- */
    uint16_t i;
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_FLS_READ, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             if (cfg->Type == FAULT_PARAMETER_CORRUPTION) {
                 /* Corrupt a parameter: Set Target Pointer to NULL to test [FLS158] */
                 printf("[TEST] Corrupted Fls_Read parameter: TargetAddressPtr -> NULL\n");
                 /* Call with NULL pointer - FLS should reject it */
                 return Fls_Read(SourceAddress, NULL_PTR, Length);
             }
        }
    }
    
    /* --- NO PARAMETER CORRUPTION - Normal data injection flow --- */
    /* Step 1: Call the real Fls_Read first to get data from flash */
    Std_ReturnType result = Fls_Read(SourceAddress, TargetAddressPtr, Length);
    
    if (result != E_OK) {
        return result;  /* Read failed, no point injecting */
    }

    /* Step 2: If a FLS fault is active, corrupt the read-back data */
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FLS_READ, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);
            
            if (cfg->Type == FAULT_RETURN_VALUE_CORRUPTION) {
                /* Skip return value corruption at this stage, handle it at the end */
                continue;
            }

            /* Skip Fee headers — only corrupt user data */
            if (IsFeeHeader(TargetAddressPtr, Length) == FALSE) {
                if (Length >= 32) {
                    Fault_Inject(TargetAddressPtr, Length, cfg);
                }
            }
        }
    }

    /* Step 3: Check for return value corruption after data injection */
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_FLS_READ, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             if (cfg->Type == FAULT_RETURN_VALUE_CORRUPTION) {
                 /* Corrupt the return value by flipping it */
                 printf("[TEST] Corrupted Fls_Read return value: 0x%02X -> ", result);
                 result = (result == E_OK) ? E_NOT_OK : E_OK;
                 printf("0x%02X\n", result);
             }
        }
    }

    return result;
}

/* --- FLS ERASE HOOK IMPLEMENTATION --- */
Std_ReturnType Hook_Fls_Erase(uint32 TargetAddress, uint32 Length) {
    
    uint16_t i;
    Std_ReturnType result;
    
    /* --- CHECK FOR PARAMETER CORRUPTION FIRST --- */
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FLS_ERASE, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);
            if (cfg->Type == FAULT_PARAMETER_CORRUPTION) {
                /* Corrupt address alignment [FLS020] */
                printf("[TEST] Corrupted Fls_Erase parameter: TargetAddress -> Unaligned (0x%X)\n", TargetAddress);
                uint32 unalignedAddr = TargetAddress + 1;  /* Make it unaligned */
                return Fls_Erase(unalignedAddr, Length);
            }
        }
    }

    /* --- NO PARAMETER CORRUPTION - Check for length corruption or normal erase --- */
    boolean faultActive = FALSE;
    FaultConfig_t* activeFaultCfg = NULL;

    /* Check if any FLS fault is active */
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FLS_ERASE, i) == TRUE) {
            faultActive = TRUE;
            activeFaultCfg = FaultState_GetConfig(i);
            break;
        }
    }

    if (faultActive == TRUE && activeFaultCfg != NULL) {
        
        if (activeFaultCfg->Type == FAULT_PARTIAL_ERASE) {
             /* User's logic: Limit the erase length */
             uint32 limitedLength = Length;
             if (activeFaultCfg->MaxEraseBytes < Length) {
                 limitedLength = activeFaultCfg->MaxEraseBytes;
             }
             result = Fls_Erase(TargetAddress, limitedLength);
        } else if (activeFaultCfg->Type == FAULT_RETURN_VALUE_CORRUPTION) {
            /* For return value corruption, execute normally first */
            result = Fls_Erase(TargetAddress, Length);
        } else if (activeFaultCfg->Type != FAULT_PARAMETER_CORRUPTION) {
            /* Fallback to original simulation: erase all but leave 1 dirty byte */
            result = Fls_Erase(TargetAddress, Length);
            VirtualFlashMemory[TargetAddress] = 0x00; 
        } else {
            result = Fls_Erase(TargetAddress, Length);
        }
    } else {
        /* No fault active — normal erase */
        result = Fls_Erase(TargetAddress, Length);
    }

    /* --- CHECK FOR RETURN VALUE CORRUPTION --- */
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_FLS_ERASE, i) == TRUE) {
             FaultConfig_t* cfg = FaultState_GetConfig(i);
             if (cfg->Type == FAULT_RETURN_VALUE_CORRUPTION) {
                 /* Corrupt the return value by flipping it */
                 printf("[TEST] Corrupted Fls_Erase return value: 0x%02X -> ", result);
                 result = (result == E_OK) ? E_NOT_OK : E_OK;
                 printf("0x%02X\n", result);
             }
        }
    }

    return result;
}