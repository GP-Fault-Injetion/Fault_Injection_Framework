#include "MemIf.h"
#include "Fee.h"

/* =========================================================================
 * WRAPPER FUNCTIONS
 * These adapt the Fee API (no DeviceIndex) to the MemIf API (has DeviceIndex).
 * ========================================================================= */

Std_ReturnType Fee_Read_Wrapper(uint8 DeviceIndex, uint16 BlockNumber, uint16 BlockOffset, uint8 *DataBufferPtr, uint16 Length) {
    (void)DeviceIndex; /* Unused */
    return Fee_Read(BlockNumber, BlockOffset, DataBufferPtr, Length);
}

Std_ReturnType Fee_Write_Wrapper(uint8 DeviceIndex, uint16 BlockNumber, uint8 *DataBufferPtr) {
    (void)DeviceIndex; /* Unused */
    return Fee_Write(BlockNumber, DataBufferPtr);
}

Std_ReturnType Fee_EraseImmediateBlock_Wrapper(uint8 DeviceIndex, uint16 BlockNumber) {
    (void)DeviceIndex; /* Unused */
    return Fee_EraseImmediateBlock(BlockNumber);
}

Std_ReturnType Fee_InvalidateBlock_Wrapper(uint8 DeviceIndex, uint16 BlockNumber) {
    (void)DeviceIndex; /* Unused */
    return Fee_InvalidateBlock(BlockNumber);
}

void Fee_Cancel_Wrapper(uint8 DeviceIndex) {
    (void)DeviceIndex; /* Unused */
    Fee_Cancel();
}

MemIf_StatusType Fee_GetStatus_Wrapper(uint8 DeviceIndex) {
    (void)DeviceIndex; /* Unused */
    return Fee_GetStatus();
}

MemIf_JobResultType Fee_GetJobResult_Wrapper(uint8 DeviceIndex) {
    (void)DeviceIndex; /* Unused */
    return Fee_GetJobResult();
}

/* =========================================================================
 * DISPATCH TABLE
 * ========================================================================= */

const MemIf_MemHwA_FunctionsType MemIf_MemHwA_Functions[MEMIF_NUMBER_OF_DEVICES] = {
    {
        /* We point to the WRAPPERS, not the direct Fee functions */
        .Read = Fee_Read_Wrapper,
        .Write = Fee_Write_Wrapper,
        .EraseImmediateBlock = Fee_EraseImmediateBlock_Wrapper,
        .InvalidateBlock = Fee_InvalidateBlock_Wrapper,
        .Cancel = Fee_Cancel_Wrapper,
        .GetStatus = Fee_GetStatus_Wrapper,
        .GetJobResult = Fee_GetJobResult_Wrapper
    }
};