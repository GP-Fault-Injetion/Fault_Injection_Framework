#include "NvM.h"
#include "NvM_ConfigTypes.h"

/* RAM Mirrors for the Blocks */
uint8 RamBlock1_Reserved[64];
uint8 RamBlock2_Test[64];
uint8 RamBlock3_Test[64];
uint8 RamBlock4_Test[64];

uint8 RomBlock3_Default[64] = {
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD,
    0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD
};

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

    /* Block 2: Redundant Block (Case 1) */
    {
        .NvramBlockIdentifier = 2,
        .BlockManagementType = NVM_BLOCK_REDUNDANT,
        .BlockJobPriority = 0,
        .BlockWriteProt = FALSE,
        .WriteBlockOnce = FALSE,
        .SelectBlockForReadall = TRUE,
        .ResistantToChangesSw = FALSE,
        .NvBlockLength = 64,
        .BlockUseCrc = TRUE,
        .BlockCRCType = NVM_CRC16,
        .RamBlockDataAddress = RamBlock2_Test,
        .CalcRamBlockCrc = TRUE,
        .NvBlockNum = 2,        /* Redundant has 2 NV blocks */
        .NvramDeviceId = 0,
        .NvBlockBaseNumber=2,
        .RomBlockDataAdress = NULL
    },

    /* Block 3: Native with ROM Default (Case 2) */
    {
        .NvramBlockIdentifier = 3,
        .BlockManagementType = NVM_BLOCK_NATIVE,
        .BlockJobPriority = 0,
        .BlockWriteProt = FALSE,
        .WriteBlockOnce = FALSE,
        .SelectBlockForReadall = TRUE,
        .ResistantToChangesSw = FALSE,
        .NvBlockLength = 64,
        .BlockUseCrc = TRUE,
        .BlockCRCType = NVM_CRC16,
        .RamBlockDataAddress = RamBlock3_Test,
        .CalcRamBlockCrc = TRUE,
        .NvBlockNum = 1,
        .NvramDeviceId = 0,
        .NvBlockBaseNumber=4,
        .RomBlockDataAdress = RomBlock3_Default
    },

    /* Block 4: Native without ROM Default (Case 3) */
    {
        .NvramBlockIdentifier = 4,
        .BlockManagementType = NVM_BLOCK_NATIVE,
        .BlockJobPriority = 0,
        .BlockWriteProt = FALSE,
        .WriteBlockOnce = FALSE,
        .SelectBlockForReadall = TRUE,
        .ResistantToChangesSw = FALSE,
        .NvBlockLength = 64,
        .BlockUseCrc = TRUE,
        .BlockCRCType = NVM_CRC16,
        .RamBlockDataAddress = RamBlock4_Test,
        .CalcRamBlockCrc = TRUE,
        .NvBlockNum = 1,
        .NvramDeviceId = 0,
        .NvBlockBaseNumber=5,
        .RomBlockDataAdress = NULL
    }
};

const NvM_ConfigType NvM_Config = {
    .Common = {
        .MultiBlockCallback = NULL
    },
    .BlockDescriptor = BlockDescriptors
};