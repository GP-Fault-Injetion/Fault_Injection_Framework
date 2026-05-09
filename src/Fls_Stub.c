#include "Fls.h"
#include <string.h>

/* 20KB Virtual Flash Memory */
#define FLASH_SIZE 32768 
static uint8 VirtualFlashMemory[FLASH_SIZE];
static MemIf_JobResultType Fls_JobResult = MEMIF_JOB_OK;

/* Define the global flag required by Fee */
volatile boolean FlsJobReady = TRUE;

void Fls_Init(const void* ConfigPtr) {
    memset(VirtualFlashMemory, FLS_ERASE_VALUE, FLASH_SIZE);
    Fls_JobResult = MEMIF_JOB_OK;
    FlsJobReady = TRUE;
}

Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length) {
    if ((TargetAddress + Length) > FLASH_SIZE) return E_NOT_OK;
    memset(&VirtualFlashMemory[TargetAddress], FLS_ERASE_VALUE, Length);
    
    Fls_JobResult = MEMIF_JOB_OK;
    FlsJobReady = TRUE; /* Operation done immediately in stub */
    return E_OK;
}

Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length) {
    if ((TargetAddress + Length) > FLASH_SIZE) return E_NOT_OK;
    int x=0;
    while(x<100)
    {
        x++;
    }
    memcpy(&VirtualFlashMemory[TargetAddress], SourceAddressPtr, Length);
    
    Fls_JobResult = MEMIF_JOB_OK;
    FlsJobReady = TRUE;
    return E_OK;
}

Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length) {
    if ((SourceAddress + Length) > FLASH_SIZE) return E_NOT_OK;
        int x=0;
    while(x<100)
    {
        x++;
    }
    memcpy(TargetAddressPtr, &VirtualFlashMemory[SourceAddress], Length);
    
    Fls_JobResult = MEMIF_JOB_OK;
    FlsJobReady = TRUE;
    return E_OK;
}

void Fls_MainFunction(void) {
    /* Ensure it stays Ready */
    Fls_JobResult = MEMIF_JOB_OK; 
    FlsJobReady = TRUE;
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