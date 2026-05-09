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
       We scan the first 16 bytes to be sure. */
    if (len < 12) return FALSE;

    uint32_t* words = (uint32_t*)data;
    int i;
    for (i = 0; i < 4 && i < (int)(len / 4); i++) {
        if (words[i] == 0xEBBABABE || words[i] == 0x2345BABE) {
            return TRUE;
        }
    }
    return FALSE;
}

/* --- HELPER: Find first active fault for a given target --- */
static FaultConfig_t* FindActiveFault(uint16_t targetId) {
    uint16_t i;
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(targetId, i) == TRUE) {
            return FaultState_GetConfig(i);
        }
    }
    return NULL;
}

/* --- NVM WRITE HOOK --- */
Std_ReturnType Hook_NvM_WriteBlock( NvM_BlockIdType blockId, const void *NvM_SrcPtr ) {
    uint16_t i;
    uint8* mutableDataPtr = (uint8*)NvM_SrcPtr;

    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (FaultState_IsActive(TARGET_NVM_WRITE_BLOCK, i) == TRUE) {
            FaultConfig_t* cfg = FaultState_GetConfig(i);
            uint32_t dataLength = GetNvMBlockLength(blockId);
            if (dataLength > 0) {
                Fault_Inject(mutableDataPtr, dataLength, cfg);
            }
        }
    }
    return NvM_WriteBlock(blockId, NvM_SrcPtr);
}

/* --- NVM READ HOOK --- */
Std_ReturnType Hook_NvM_ReadBlock( NvM_BlockIdType blockId, void *NvM_DstPtr ) {
    return NvM_ReadBlock(blockId, NvM_DstPtr);
}

/* --- FLS WRITE HOOK --- */
Std_ReturnType Hook_Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length) {
    uint8 tempBuffer[512];
    uint32 actualLen = (Length > 512) ? 512 : Length;
    Std_ReturnType result;

    FaultConfig_t* cfg = FindActiveFault(TARGET_FLS_WRITE);

    if (cfg == NULL) {
        return Fls_Write(TargetAddress, SourceAddressPtr, Length);
    }

    switch (cfg->Type) {
        case FAULT_PARAMETER_CORRUPTION:
            printf("[TEST] Corrupted Fls_Write parameter: SourceAddressPtr -> NULL\n");
            return Fls_Write(TargetAddress, NULL_PTR, Length);

        case FAULT_RETURN_VALUE_REJECTION:
            printf("[TEST] Fls_Write return value REJECTED (no real call): -> 0x%02X\n", E_NOT_OK);
            return E_NOT_OK;

        case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
            result = Fls_Write(TargetAddress, SourceAddressPtr, Length);
            printf("[TEST] Corrupted Fls_Write return value: 0x%02X -> ", result);
            result = (result == E_OK) ? E_NOT_OK : E_OK;
            printf("0x%02X\n", result);
            return result;

        default:
            if (SourceAddressPtr != NULL
                && IsFeeHeader(SourceAddressPtr, actualLen) == FALSE
                && Length >= 32) {
                memcpy(tempBuffer, SourceAddressPtr, actualLen);
                Fault_Inject(tempBuffer, actualLen, cfg);
                return Fls_Write(TargetAddress, tempBuffer, actualLen);
            }
            return Fls_Write(TargetAddress, SourceAddressPtr, Length);
    }
}

/* --- FLS READ HOOK --- */
Std_ReturnType Hook_Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length) {
    Std_ReturnType result;

    FaultConfig_t* cfg = FindActiveFault(TARGET_FLS_READ);

    if (cfg == NULL) {
        return Fls_Read(SourceAddress, TargetAddressPtr, Length);
    }

    switch (cfg->Type) {
        case FAULT_PARAMETER_CORRUPTION:
            printf("[TEST] Corrupted Fls_Read parameter: TargetAddressPtr -> NULL\n");
            return Fls_Read(SourceAddress, NULL_PTR, Length);

        case FAULT_RETURN_VALUE_REJECTION:
            printf("[TEST] Fls_Read return value REJECTED (no real call): -> 0x%02X\n", E_NOT_OK);
            return E_NOT_OK;

        case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
            result = Fls_Read(SourceAddress, TargetAddressPtr, Length);
            printf("[TEST] Corrupted Fls_Read return value: 0x%02X -> ", result);
            result = (result == E_OK) ? E_NOT_OK : E_OK;
            printf("0x%02X\n", result);
            return result;

        default:
            result = Fls_Read(SourceAddress, TargetAddressPtr, Length);
            if (result == E_OK
                && TargetAddressPtr != NULL
                && IsFeeHeader(TargetAddressPtr, Length) == FALSE
                && Length >= 32) {
                Fault_Inject(TargetAddressPtr, Length, cfg);
            }
            return result;
    }
}

/* --- FLS ERASE HOOK --- */
Std_ReturnType Hook_Fls_Erase(uint32 TargetAddress, uint32 Length) {
    Std_ReturnType result;

    FaultConfig_t* cfg = FindActiveFault(TARGET_FLS_ERASE);

    if (cfg == NULL) {
        return Fls_Erase(TargetAddress, Length);
    }

    switch (cfg->Type) {
        case FAULT_PARAMETER_CORRUPTION: {
            uint32 unalignedAddr = TargetAddress + 1;
            printf("[TEST] Corrupted Fls_Erase parameter: TargetAddress -> Unaligned (0x%X)\n", TargetAddress);
            return Fls_Erase(unalignedAddr, Length);
        }

        case FAULT_RETURN_VALUE_REJECTION:
            printf("[TEST] Fls_Erase return value REJECTED (no real call): -> 0x%02X\n", E_NOT_OK);
            return E_NOT_OK;

        case FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION:
            result = Fls_Erase(TargetAddress, Length);
            printf("[TEST] Corrupted Fls_Erase return value: 0x%02X -> ", result);
            result = (result == E_OK) ? E_NOT_OK : E_OK;
            printf("0x%02X\n", result);
            return result;

        default:
            /* Erase normally but leave one dirty byte to simulate incomplete erase */
            result = Fls_Erase(TargetAddress, Length);
            VirtualFlashMemory[TargetAddress] = 0x00;
            return result;
    }
}
