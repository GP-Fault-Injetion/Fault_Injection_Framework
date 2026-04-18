#include "Fls.h"
#include <string.h>

/* 20KB Virtual Flash Memory */
#define FLASH_SIZE      32768
uint8 VirtualFlashMemory[FLASH_SIZE];
static MemIf_JobResultType Fls_JobResult = MEMIF_JOB_OK;

/* Define the global flag required by Fee */
volatile boolean FlsJobReady = TRUE;

/* Variables to track Async Jobs */
static uint32 LastJobAddress = 0;
static uint32 LastJobLength = 0;
static boolean IsEraseJobPending = FALSE;

void Fls_Init(const void* ConfigPtr) {
    memset(VirtualFlashMemory, FLS_ERASE_VALUE, FLASH_SIZE);
    Fls_JobResult = MEMIF_JOB_OK;
    FlsJobReady = TRUE;
    IsEraseJobPending = FALSE;
}

Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length) {
    /* Round up to the nearest sector boundary */
    uint32 roundedLength = ((Length + FLS_SECTOR_SIZE - 1) / FLS_SECTOR_SIZE) * FLS_SECTOR_SIZE;

    if ((TargetAddress + roundedLength) > FLASH_SIZE) return E_NOT_OK;
    
    memset(&VirtualFlashMemory[TargetAddress], FLS_ERASE_VALUE, roundedLength);
    
    /* Act as Asynchronous (for MainFunction to verify) */
    LastJobAddress = TargetAddress;
    LastJobLength = roundedLength;
    IsEraseJobPending = TRUE;
    
    Fls_JobResult = MEMIF_JOB_PENDING;
    FlsJobReady = FALSE; 
    return E_OK;
}

Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length) {
    if ((TargetAddress + Length) > FLASH_SIZE) return E_NOT_OK;
    memcpy(&VirtualFlashMemory[TargetAddress], SourceAddressPtr, Length);
    
    IsEraseJobPending = FALSE;
    Fls_JobResult = MEMIF_JOB_PENDING;
    FlsJobReady = FALSE;
    return E_OK;
}

Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length) {
    if ((SourceAddress + Length) > FLASH_SIZE) return E_NOT_OK;
    memcpy(TargetAddressPtr, &VirtualFlashMemory[SourceAddress], Length);
    
    IsEraseJobPending = FALSE;
    Fls_JobResult = MEMIF_JOB_PENDING;
    FlsJobReady = FALSE;
    return E_OK;
}

void Fls_MainFunction(void) {
    if (Fls_JobResult == MEMIF_JOB_PENDING) {
        
        if (IsEraseJobPending) {
            /* FLS022 Requirements: Verify erased block */
            uint32 i;
            boolean verifyPassed = TRUE;
            for (i = 0; i < LastJobLength; i++) {
                if (VirtualFlashMemory[LastJobAddress + i] != FLS_ERASE_VALUE) {
                    verifyPassed = FALSE;
                    break;
                }
            }

            if (verifyPassed) {
                Fls_JobResult = MEMIF_JOB_OK;
            } else {
                Fls_JobResult = MEMIF_JOB_FAILED;
            }
            IsEraseJobPending = FALSE;
        } else {
            /* For Read/Write, just complete immediately */
            Fls_JobResult = MEMIF_JOB_OK;
        }
        
        FlsJobReady = TRUE;
    }
}

MemIf_JobResultType Fls_GetJobResult(void) {
    return Fls_JobResult;
}



/* Fee calls this to check if the Flash hardware is busy. 
 * Since our stub is instant, we are always IDLE (Ready). 
 */
MemIf_StatusType Fls_GetStatus(void) {
    return MEMIF_IDLE;
}

/* Fee calls this to switch between Fast/Slow mode.
 * We ignore it for simulation.
 */
void Fls_SetMode(MemIf_ModeType Mode) {
    (void)Mode; /* Prevent unused variable warning */
}