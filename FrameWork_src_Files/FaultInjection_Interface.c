#include "FaultInjection_Interface.h"

Std_ReturnType Fault_Init(void) {
    FaultState_Init();
    /* Seed random number generator for data logic */
    Fault_SeedRandom(0x12345678); 
    return E_OK;
}

Std_ReturnType Fault_Inject(uint8_t* data, uint32_t length, FaultConfig_t* config) {
    if ((data == NULL) || (config == NULL)) {
        return E_NOT_OK;
    }

    /* Safety check: Only inject if the fault is actually Active */
    if (config->Active == FALSE) {
        return E_OK; /* Fault configured but currently outside duration window */
    }

    Std_ReturnType status = E_OK;

    switch (config->Type) {
        case FAULT_BIT_FLIP:
        case FAULT_MULTI_BIT_FLIP:
        case FAULT_STUCK_AT_0:
        case FAULT_STUCK_AT_1:
            /* Delegate to Bit Logic */
            status = ApplyBitFault(data, length, config);
            break;

        case FAULT_DATA_CORRUPTION:
            /* Randomize or Burst Error */
            /* Using Burst Error as generic corruption for this example */
            status = Fault_InjectBurstError(data, length, 64); // Default burst 4 bytes
            break;

        case FAULT_CRC_DATA_CORRUPTION:
            /* Corrupt the last byte(s) assuming they are CRC */
            status = Fault_CorruptCRCField(data, length, 16); // Assuming 16-bit CRC
            break;

        case FAULT_NONE:
        default:
            /* No action */
            break;
    }

    return status;
}

void Fault_Clear(uint16_t TargetModuleServiceID) {
    FaultState_Clear(TargetModuleServiceID);
}