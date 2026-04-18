#include "NvM.h"
#include "NvM_ConfigTypes.h"

/* RAM Mirrors for the Blocks */
uint8 RamBlock1_Reserved[64];
uint8 RamBlock2_Test[64];
uint8 RamBlock3_Test[64];

const NvM_BlockDescriptorType BlockDescriptors[] = {
    /* Block 0: MultiBlock (Required by AUTOSAR, usually reserved) */
    {
        .NvramBlockIdentifier = 0,
        .BlockManagementType = NVM_BLOCK_NATIVE,
        .RamBlockDataAddress = NULL, 
        .NvBlockLength = 0
        /* Other fields 0/NULL */
    },

    /* Block 1: Reserved Block */
    {
        .NvramBlockIdentifier = 1,
        .BlockManagementType = NVM_BLOCK_NATIVE,
        .BlockJobPriority = 0,
        .BlockWriteProt = FALSE,
        .WriteBlockOnce = FALSE,
        .SelectBlockForReadall = TRUE,
        .ResistantToChangesSw = FALSE,
        .NvBlockLength = 64,
        .BlockUseCrc = TRUE,
        .BlockCRCType = NVM_CRC16,
        .RamBlockDataAddress = RamBlock1_Reserved,
        .CalcRamBlockCrc = TRUE,
        .NvBlockNum = 1,        /* Maps to Fee Block 1 */
        .NvramDeviceId = 0,     /* 0 = Fee */
        .NvBlockBaseNumber = 1     
    },

    /* Block 2: OUR TEST BLOCK */
    {
        .NvramBlockIdentifier = 2,
        .BlockManagementType = NVM_BLOCK_NATIVE,
        .BlockJobPriority = 0,
        .BlockWriteProt = FALSE,
        .WriteBlockOnce = FALSE,
        .SelectBlockForReadall = TRUE,
        .ResistantToChangesSw = FALSE,
        .NvBlockLength = 64,
        .BlockUseCrc = TRUE,
        .BlockCRCType = NVM_CRC16,
        .CalcRamBlockCrc = TRUE,
        .NvBlockNum = 2,        /* Maps to Fee Block 2 */
        .NvramDeviceId = 0 ,/* 0 = Fee */
        .NvBlockBaseNumber=2     
    }
};

const NvM_ConfigType NvM_Config = {
    .Common = {
        .MultiBlockCallback = NULL
    },
    .BlockDescriptor = BlockDescriptors
};