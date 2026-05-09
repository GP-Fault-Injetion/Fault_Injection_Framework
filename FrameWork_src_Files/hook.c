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
#include "Fault_state.h"
#include "FaultInjection_Interface.h"
#include "Fee_ConfigTypes.h"

typedef enum {
    FEE_STATUS_FAULT_BUSY_TO_IDLE ,
    FEE_STATUS_FAULT_IDLE_TO_BUSY  ,
    FEE_STATUS_FAULT_IDLE_TO_UNINIT ,
    FEE_STATUS_FAULT_TO_BUSY_INTERNAL       
}state_corruption;


//nvm
#undef NvM_WriteBlock
#undef NvM_ReadBlock
/*------------------------------- */ 
//fls
#undef Fls_Write
/*--------------------------------*/ 
//fee
#undef Fee_Write
#undef Fee_GetJobResult
#undef Fee_GetStatus
#undef Fee_Read
#undef Fee_GetStatus
/*--------------------------------*/


extern Std_ReturnType NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr );	
extern Std_ReturnType NvM_WriteBlock( NvM_BlockIdType blockId, const void *NvM_SrcPtr );	
extern Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
extern Std_ReturnType Fee_Write(uint16 blockNumber, uint8* dataBufferPtr);
extern Std_ReturnType Fee_Read(uint16 blockNumber, uint16 blockOffset, uint8* dataBufferPtr, uint16 length);
extern MemIf_StatusType Fee_GetStatus(void);
extern MemIf_JobResultType Fee_GetJobResult(void);

extern const NvM_ConfigType NvM_Config;
extern FaultConfig_t* FaultState_GetConfig(uint16_t fault_Id);

//Helper to print hte buffer 
void PrintBuffer2(const char* label, uint8* buf, uint8* reference) {
    printf("%s:\n", label);
    for(int i = 0; i < 66; i++) {   // print first 66 bytes only
        if (reference != NULL && buf[i] != reference[i]) {
            printf("[0x%02X] ", buf[i]);  // Corrupted byte in brackets
        } else {
            printf("0x%02X ", buf[i]);     // Normal byte
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
    
    uint8 tempBuffer[512]; 
    uint32 actualLen = (Length > 512) ? 512 : Length;
    

    memcpy(tempBuffer, SourceAddressPtr, actualLen);
    

    uint16_t i;
    for(i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if(FaultState_IsActive(TARGET_FLS_WRITE, i) == TRUE) {
            
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
/*---------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------*/
//fee helper functions 
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
        phase,
        Fee_GetStatus(),
        Fee_GetJobResult());
}

// fee hooks write 
Std_ReturnType Hook_Fee_Write(uint16 blockNumber, uint8* dataBufferPtr) {
    uint32 dataLength = GetFeeBlockLength(blockNumber);
    uint16 i;
    
    
    // PrintBuffer2("FEE Write Data", dataBufferPtr, NULL);

    if ((dataBufferPtr == NULL) || (dataLength == 0)) {
        return Fee_Write(blockNumber, dataBufferPtr);
    }

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FEE_WRITE, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);
            if(cfg->Type==FAULT_BIT_FLIP || cfg->Type==FAULT_MULTI_BIT_FLIP||
                cfg->Type==FAULT_STUCK_AT_0 || cfg->Type==FAULT_STUCK_AT_1|| 
                cfg->Type==FAULT_DATA_CORRUPTION ||
                cfg->Type==FAULT_CRC_DATA_CORRUPTION) {
            
                Fault_Inject(dataBufferPtr, dataLength, cfg);
            }

            else if (cfg->Type == FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION) {
            Std_ReturnType ret = Fee_Write(blockNumber, dataBufferPtr);
            PrintFeeRuntime("status after real Fee_Write call in FEE case");
            return (ret == E_OK) ? E_NOT_OK : E_OK;
            
            }
            else if(cfg->Type == FAULT_RETURN_VALUE_REJECTION) {
                

                return E_NOT_OK; 
            }
            else if(cfg->Type == FAULT_OMISSION) {
            
                return E_OK; 
            }
        }
    }

    return Fee_Write(blockNumber, dataBufferPtr);
}/*==========================================================================*/

Std_ReturnType Hook_Fee_Read(uint16 blockNumber, uint16 blockOffset, uint8* dataBufferPtr, uint16 length) {
    
    uint32 dataLength = GetFeeBlockLength(blockNumber);
    uint16 i;
    
    
    // PrintBuffer2("FEE Write Data", dataBufferPtr, NULL);

    if ((dataBufferPtr == NULL) || (dataLength == 0)) {
        return Fee_Read(blockNumber, blockOffset, dataBufferPtr, length);
    }

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FEE_READ, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);
            if(cfg->Type==FAULT_BIT_FLIP || cfg->Type==FAULT_MULTI_BIT_FLIP||
                cfg->Type==FAULT_STUCK_AT_0 || cfg->Type==FAULT_STUCK_AT_1|| 
                cfg->Type==FAULT_DATA_CORRUPTION ||
                cfg->Type==FAULT_CRC_DATA_CORRUPTION) {
            
                //still not know how to do it
            }

            else if (cfg->Type == FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION) {
            Std_ReturnType ret = Fee_Read(blockNumber, blockOffset, dataBufferPtr, length);
            PrintFeeRuntime("status after real Fee_Read call in FEE case");
            return (ret == E_OK) ? E_NOT_OK : E_OK;
            
            }
            else if(cfg->Type == FAULT_RETURN_VALUE_REJECTION) {
                

                return E_NOT_OK; 
            }
            else if(cfg->Type == FAULT_PARAMETER_CORRUPTION) {
                blockNumber = 0xFFFFU;
                return Fee_Read(blockNumber, blockOffset, dataBufferPtr, length); 
            }
            else if (cfg->Type == FAULT_OMISSION) {
            
                return E_OK; 
            }
        }
    }

    return Fee_Read(blockNumber, blockOffset, dataBufferPtr, length);

}

/*========================================================================*/
MemIf_JobResultType Hook_Fee_GetJobResult(void) {
    uint16 i;
    MemIf_JobResultType returnedResult;
    //printf("[Hook] Hook_Fee_GetJobResult called\n");
    //printf("[Hook] the real Fee_GetJobResult returned: %d\n============================================",Fee_GetJobResult());


    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_FEE_GET_JOB_RESULT, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);
            
            if (cfg->Type == FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION) {
                // Capture the actual result once
                returnedResult = Fee_GetJobResult();
               // printf("[Hook] the real Fee_GetJobResult returned: %d\n============================================", returnedResult);
                if(returnedResult == MEMIF_JOB_PENDING){
                    

                       // printf("[Hook] Fee_GetJobResult: Corrupting from MEMIF_JOB_PENDING to MEMIF_JOB_OK\n");
                        return MEMIF_JOB_OK;}
                    else
                        // Pass through the original, uncorrupted result
                        return returnedResult;
                }
            }
        }
            // Normal execution path if no faults are active for this function
    return Fee_GetJobResult(); 
    }

/*==========================================================================================
=============================================================================================*/
MemIf_StatusType Hook_Fee_GetStatus(void)
{
    uint16 i;
    MemIf_StatusType realStatus;

    realStatus = Fee_GetStatus();

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++)
    {
        if (FaultState_IsActive(TARGET_FEE_GET_STATUS, i) == TRUE)
        {
            FaultConfig_t* cfg = FaultState_GetConfig(i);

            if ((cfg != NULL) && (cfg->Type == FAULT_MODULE_STATE_CORRUPTION))
            {
                
                if (realStatus == MEMIF_BUSY)
                {
                    //printf("[STATE CORRUPTION][FEE] Fee_GetStatus: real=MEMIF_BUSY corrupted=MEMIF_IDLE\n");
                    return MEMIF_IDLE;
                }
                

            
                else if (realStatus == MEMIF_IDLE)
                {
                    //printf("[STATE CORRUPTION][FEE] Fee_GetStatus: real=MEMIF_IDLE corrupted=MEMIF_BUSY\n");
                    return MEMIF_BUSY;
                }
                

            
                // else if (realStatus == MEMIF_IDLE)
                // {
                //     printf("[STATE CORRUPTION][FEE] Fee_GetStatus: real=MEMIF_IDLE corrupted=MEMIF_UNINIT\n");
                //     return MEMIF_UNINIT;
                // }
                
            }
        }
    }

    return realStatus;
}
