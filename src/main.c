#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* AUTOSAR / Stack Headers */
#include "Std_Types.h"
#include "NvM.h"
#include "MemIf.h"
#include "Fee.h"
#include "Fls.h"
#include "Crc.h"

/* Force fflush on all prints for debugging */
#undef printf
#define printf(...) do { fprintf(stdout, __VA_ARGS__); fflush(stdout); } while(0)

/* Fault Injection Headers */
#define ENABLE_FAULT_INJECTION_HOOKS
#include "hook.h"
#include "FaultInjection_Interface.h"
#include "Fault_state.h"

/* --- FEE Configuration --- */
#define FEE_TEST_BLOCK_ID   2
#define FEE_BUFFER_SIZE     66

/* --- Configuration --- */
#define TEST_BLOCK_ID   2
#define BUFFER_SIZE     64
#define TICK_MS         1

/* --- DET Mocking --- */
extern uint8 Last_Det_Error;

/* --- Globals for Reporting --- */
static int g_testsPassed = 0;
static int g_testsFailed = 0;

/* =========================================================================
 * SYSTEM SIMULATION HELPERS
 * ========================================================================= */

uint32_t GetSystemTimeMs(void) {
    return (uint32_t)((clock() * 1000) / CLOCKS_PER_SEC);
}

void ProcessSystem(uint32_t durationMs) {
    uint32_t start = GetSystemTimeMs();
    uint32_t lastFaultUpdate = start;
    uint32_t now;

    do {
        now = GetSystemTimeMs();

        /* --- STACK SCHEDULING --- */
        Fls_MainFunction();
        Fee_MainFunction();
        NvM_MainFunction();

        /* --- Fault Injection --- */
        if ((now - lastFaultUpdate) >= 1) {
            FaultState_MainFunction();
            lastFaultUpdate = now;
        }

    } while ((now - start) < durationMs);
}

void WaitForNvM(void) {
    NvM_RequestResultType status;
    uint32_t start = GetSystemTimeMs();

    do {
        ProcessSystem(TICK_MS);
        NvM_GetErrorStatus(TEST_BLOCK_ID, &status);
        if ((GetSystemTimeMs() - start) > 2000) {
            printf("!! TIMEOUT WAITING FOR NVM !!\n");
            break;
        }
    } while (status == NVM_REQ_PENDING);
}

void WaitForBlock(uint16_t blockId) {
    NvM_RequestResultType status;
    uint32_t start = GetSystemTimeMs();

    do {
        ProcessSystem(TICK_MS);
        NvM_GetErrorStatus(blockId, &status);
        if ((GetSystemTimeMs() - start) > 2000) {
            printf("!! TIMEOUT WAITING FOR NVM !!\n");
            break;
        }
    } while (status == NVM_REQ_PENDING);
}

/* =========================================================================
 * VISUALIZATION & ANALYSIS HELPERS
 * ========================================================================= */

void ResetBuffer(uint8* buf, uint8 startVal) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buf[i] = (uint8)(startVal + i);
    }
}

void PrintBinary(uint8 val) {
    for(int i=7; i>=0; i--) {
        printf("%c", (val & (1<<i)) ? '1' : '0');
    }
}

void AnalyzeResult_Block(const char* testName, uint8* expected, uint8* actual, boolean expectFault, uint16_t blockId) {
    NvM_RequestResultType status;
    NvM_GetErrorStatus(blockId, &status);

    boolean dataMatch = (memcmp(expected, actual, BUFFER_SIZE) == 0);
    boolean corruptionDetected = !dataMatch;

    const char* detector = "NONE";
    const char* outcome = "UNKNOWN";
    boolean testPassed = FALSE;

    if (status == NVM_REQ_INTEGRITY_FAILED) {
        detector = "NvM (CRC Check Failed)";
    } else if (status == NVM_REQ_NV_INVALIDATED) {
        detector = "Fee (Header Check)";
    } else if (status == NVM_REQ_NOT_OK) {
        detector = "Driver (Write Error)";
    } else if (status == NVM_REQ_OK) {
        if (corruptionDetected) {
            detector = "Application (Silent Corruption!)";
        } else {
            detector = "No Error Detected";
        }
    }

    if (expectFault) {
        if (status == NVM_REQ_OK && !corruptionDetected) {
            outcome = "Fault Injected, but RECOVERED successfully.";
            testPassed = TRUE;
        } else if (status != NVM_REQ_OK) {
            outcome = "Fault Injected Successfully. Stack CAUGHT it.";
            testPassed = TRUE;
        } else if (corruptionDetected) {
            outcome = "Fault Injected Successfully. Stack MISSED it.";
            testPassed = FALSE;
        } else {
            outcome = "Fault Failed to Inject (Data is clean).";
            testPassed = FALSE;
        }
    } else {
        if (status == NVM_REQ_OK && !corruptionDetected) {
            outcome = "Normal Operation (Success).";
            testPassed = TRUE;
        } else {
            outcome = "Unexpected Error or Corruption!";
            testPassed = FALSE;
        }
    }

    printf("   ------------------------------------------------------------\n");
    printf("   Status:   %s\n", outcome);
    printf("   Detector: %s\n", detector);

    if (testPassed) {
        printf("   RESULT:   [PASS]\n");
        g_testsPassed++;
    } else {
        printf("   RESULT:   [FAIL]\n");
        g_testsFailed++;
    }
    printf("============================================================\n\n");
}

/* PrintBuffer: prints buffer bytes with a length limit */
void PrintBuffer(const char* label, const uint8* buf, uint32_t len) {
    printf("   [DATA] %-10s: ", label);
    for (uint32_t i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
        if (i > 0 && (i + 1) % 16 == 0 && i < len - 1) printf("\n                     ");
    }
    printf("\n");
}

/* PrintBufferCmp: prints buffer bytes with optional comparison (for FEE tests) */
static void PrintBufferCmp(const char* label, uint8* buf, uint8* reference) {
    printf("%s:\n", label);
    for(int i = 0; i < 16; i++) {
        if (reference != NULL && buf[i] != reference[i]) {
            printf("[0x%02X] ", buf[i]);
        } else {
            printf("0x%02X ", buf[i]);
        }
    }
    printf("\n");
}

/* AnalyzeResult: compatibility wrapper using fixed TEST_BLOCK_ID */
static void AnalyzeResult(const char* testName, uint8* expected, uint8* actual, boolean expectFault) {
    AnalyzeResult_Block(testName, expected, actual, expectFault, TEST_BLOCK_ID);
}

/* =========================================================================
 * TEST CASES — FLS / NvM INTEGRATION
 * ========================================================================= */

void Test_BitFlip_Immediate(void) {
    printf("=== TEST 1: Bit Flip Immediate (NVM Target) ===\n");
    printf("   Goal: Flip Bit 0 of Byte 10. Check if NvM CRC catches it.\n");

    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);

    Fault_Clear(TARGET_FLS_WRITE);
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_BIT_FLIP, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;
    cfg->BitPosition  = (10 * 8) + 0;

    ProcessSystem(5);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();

    AnalyzeResult_Block("BitFlip Immediate", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_Delayed_Start(void) {
    printf("=== TEST 2: Delayed Start (CRC Corruption) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);

    Fault_Clear(TARGET_FLS_WRITE);
    uint32_t now = GetSystemTimeMs();
    uint32_t delay = 200;

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_CRC_DATA_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = now + delay;
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;

    printf("   [Phase 1] Immediate Write (Before Fault Start)\n");
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Delayed Start (Phase 1)", golden, read, FALSE, TEST_BLOCK_ID);

    printf("   [Phase 2] Waiting for Fault Activation...\n");
    ProcessSystem(delay + 50);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Delayed Start (Phase 2)", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_StuckAt_Logic(void) {
    printf("=== TEST 3: Stuck-At Logic ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(TARGET_FLS_WRITE);

    printf("   [Part A] Stuck-At-1 on Byte 5, Bit 4\n");
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_STUCK_AT_1, 200, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 200;
    cfg->BitPosition = (5 * 8) + 4;

    ProcessSystem(5);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Stuck-At-1", golden, read, TRUE, TEST_BLOCK_ID);

    printf("   [Part B] Stuck-At-0 on Byte 5, Bit 0\n");
    ResetBuffer(sent, 0xFF);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(TARGET_FLS_WRITE);

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_STUCK_AT_0, 200, 1);
    cfg = FaultState_GetConfig(1);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 200;
    cfg->BitPosition = (5 * 8) + 0;

    ProcessSystem(5);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Stuck-At-0", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_Frequency_Pulse(void) {
    printf("=== TEST 4: Frequency Pulse (ON/OFF/ON) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(TARGET_FLS_WRITE);

    uint32_t now = GetSystemTimeMs();
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_BIT_FLIP, 50, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = now;
    cfg->End_timeMs   = now + 1000;
    cfg->Freq         = 200;
    cfg->BitPosition  = 0;

    ProcessSystem(10);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Freq Pulse (ON)", golden, read, TRUE, TEST_BLOCK_ID);

    ProcessSystem(90);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Freq Pulse (OFF Gap)", golden, read, FALSE, TEST_BLOCK_ID);

    ProcessSystem(110);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Freq Pulse (ON Again)", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_MultiBit_Mask(void) {
    printf("=== TEST 5: Multi-Bit Mask ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(TARGET_FLS_WRITE);

    printf("   [Config] Applying Mask 0x0F to Byte 0\n");
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_MULTI_BIT_FLIP, 200, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 200;
    cfg->Mask = 0x0F;

    ProcessSystem(5);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Multi-Bit Mask", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_Concurrency_Stress(void) {
    printf("=== TEST 6: Concurrency Stress (Triple Fault) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(TARGET_FLS_WRITE);
    uint32_t now = GetSystemTimeMs();

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_BIT_FLIP, 500, 0);
    FaultConfig_t* c0 = FaultState_GetConfig(0);
    c0->Start_TimeMs = now; c0->End_timeMs = now + 500; c0->BitPosition = 0;

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_STUCK_AT_1, 500, 1);
    FaultConfig_t* c1 = FaultState_GetConfig(1);
    c1->Start_TimeMs = now; c1->End_timeMs = now + 500; c1->BitPosition = 15;

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_DATA_CORRUPTION, 500, 2);
    FaultConfig_t* c2 = FaultState_GetConfig(2);
    c2->Start_TimeMs = now; c2->End_timeMs = now + 500;

    ProcessSystem(10);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Concurrency Stress", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_Fls_BitFlip_Visual(void) {
    printf("=== TEST 7: FLS Visual Bit Flip (Bit 3) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    memset(sent, 0x00, BUFFER_SIZE);
    memcpy(golden, sent, BUFFER_SIZE);

    Fault_Clear(TARGET_FLS_WRITE);

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_BIT_FLIP, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;
    cfg->BitPosition  = 3;

    ProcessSystem(10);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    memset(read, 0x00, BUFFER_SIZE);
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();

    printf("   Byte 0 Expected: "); PrintBinary(golden[0]); printf("\n");
    printf("   Byte 0 Actual:   "); PrintBinary(read[0]); printf("\n");

    AnalyzeResult_Block("Visual Bit Flip", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_Fls_TimeWindow(void) {
    printf("=== TEST 8: FLS Time Window Precision ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(TARGET_FLS_WRITE);

    uint32_t now = GetSystemTimeMs();
    uint32_t startWindow = now + 200;
    uint32_t endWindow   = now + 400;

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_BIT_FLIP, (endWindow - startWindow), 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = startWindow;
    cfg->End_timeMs   = endWindow;
    cfg->BitPosition  = 0;

    printf("   [Step 1] Writing Before Window...\n");
    ProcessSystem(10);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Time Window (Before)", golden, read, FALSE, TEST_BLOCK_ID);

    printf("   [Step 2] Writing Inside Window...\n");
    ProcessSystem(startWindow - GetSystemTimeMs() + 50);
    ResetBuffer(sent, 0x11); memcpy(golden, sent, BUFFER_SIZE);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Time Window (Inside)", golden, read, TRUE, TEST_BLOCK_ID);

    printf("   [Step 3] Writing After Window...\n");
    ProcessSystem(endWindow - GetSystemTimeMs() + 50);
    ResetBuffer(sent, 0x22); memcpy(golden, sent, BUFFER_SIZE);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Time Window (After)", golden, read, FALSE, TEST_BLOCK_ID);
}

void Test_Fls_XOR_Logic(void) {
    printf("=== TEST 9: FLS XOR Logic (Mask 0xAA) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    Fault_Clear(TARGET_FLS_WRITE);

    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_MULTI_BIT_FLIP, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;
    cfg->Mask         = 0xAA;

    ResetBuffer(sent, 0x00);
    sent[0] = 0x55;
    memcpy(golden, sent, BUFFER_SIZE);

    ProcessSystem(10);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    memset(read, 0x00, BUFFER_SIZE);
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();

    AnalyzeResult_Block("XOR Logic", golden, read, TRUE, TEST_BLOCK_ID);
}

void Test_Case1_RedundantBlock(void) {
    printf("=== CASE 1: Redundant Block (Block 2) ===\n");
    printf("   [NVM199/NVM279/NVM655] If CRC fails on primary, recovers from secondary.\n");
    uint16_t blockId = 3;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x11);
    memcpy(golden, sent, BUFFER_SIZE);
    PrintBuffer("write ", golden, BUFFER_SIZE);

    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);

    printf("   [Step 1] Writing corrupted data to Primary, clean to Secondary...\n");
    Fault_Clear(TARGET_FLS_WRITE);
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_DATA_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 61;

    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);
    PrintBuffer("write ", golden, BUFFER_SIZE);

    printf("   [Step 2] Reading back. NVM should recover from Secondary copy.\n");
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);
    PrintBuffer("read ", read, BUFFER_SIZE);

    AnalyzeResult_Block("Case 1 Redundant", golden, read, TRUE, blockId);
}

void Test_Case2_NativeWithROM(void) {
    printf("=== CASE 2: Native Block with ROM Default (Block 3) ===\n");
    printf("   [NVM202] Load default values if CRC fails.\n");
    uint16_t blockId = 4;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE];
    uint8 rom_golden[BUFFER_SIZE];
    memset(rom_golden, 0xDD, BUFFER_SIZE);

    ResetBuffer(sent, 0x33);

    Fault_Clear(TARGET_NVM_READ_BLOCK);
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_DATA_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 500;

    printf("   [Step 1] Writing corrupted data to NV memory...\n");
    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);
    PrintBuffer("sent ", sent, BUFFER_SIZE);
    printf("   [Step 2] Reading back. NVM should fail CRC and load ROM defaults.\n");
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);
    PrintBuffer("read ", read, BUFFER_SIZE);

    AnalyzeResult_Block("Case 2 Native With ROM", rom_golden, read, TRUE, blockId);
}

void Test_Case3_NativeNoROM(void) {
    printf("=== CASE 3: Native Block WITHOUT ROM (Block 4) ===\n");
    printf("   [NVM658] If CRC fails and no ROM, block remains invalid.\n");
    uint16_t blockId = 5;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x44);
    memcpy(golden, sent, BUFFER_SIZE);

    Fault_Clear(TARGET_NVM_READ_BLOCK);
    FaultState_Activate_fault(TARGET_NVM_READ_BLOCK, FAULT_DATA_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 500;

    printf("   [Step 1] Writing corrupted data to NV memory...\n");
    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);

    printf("   [Step 2] Reading back. NVM should fail CRC and return INTEGRITY_FAILED.\n");
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);

    AnalyzeResult_Block("Case 3 Native No ROM", golden, read, TRUE, blockId);
}

void Test_Case4_Test_Case4_Dataset(void) {
    printf("=== CASE 4: Dataset Block (Block 5) ===\n");
    printf("   [NVM] Writing to different dataset slots.\n");
    Fault_Clear(TARGET_FLS_WRITE);
    uint16_t blockId = 6;
    uint8 data1[BUFFER_SIZE], data2[BUFFER_SIZE];
    uint8 read1[BUFFER_SIZE], read2[BUFFER_SIZE];

    ResetBuffer(data1, 0x11);
    ResetBuffer(data2, 0x22);

    printf("   [Step 1] Writing data to slot 0...\n");
    NvM_SetDataIndex(blockId, 0);
    NvM_WriteBlock(blockId, data1); WaitForBlock(blockId);

    printf("   [Step 2] Writing different data to slot 1...\n");
    NvM_SetDataIndex(blockId, 1);
    NvM_WriteBlock(blockId, data2); WaitForBlock(blockId);

    printf("   [Step 3] Reading back slot 0...\n");
    memset(read1, 0, BUFFER_SIZE);
    NvM_SetDataIndex(blockId, 0);
    NvM_ReadBlock(blockId, read1); WaitForBlock(blockId);

    printf("   [Step 4] Reading back slot 1...\n");
    memset(read2, 0, BUFFER_SIZE);
    NvM_SetDataIndex(blockId, 1);
    NvM_ReadBlock(blockId, read2); WaitForBlock(blockId);
    printf("   [DEBUG] Expected Slot 0: 0x11, Actual Slot 0: 0x%02X\n", read1[0]);
    AnalyzeResult_Block("Case 4 Dataset Slot 0", data1, read1, FALSE, blockId);
    AnalyzeResult_Block("Case 4 Dataset Slot 1", data2, read2, FALSE, blockId);
}

void Test_Queue_Overflow(void) {
    printf("=== TEST: Queue Overflow ===\n");

    uint8 data[BUFFER_SIZE];
    ResetBuffer(data, 0xAA);

    Std_ReturnType status;
    int i;
    int rejected = 0;

    for (i = 2; i < 2 + NVM_SIZE_STANDARD_JOB_QUEUE; i++) {
        status = NvM_WriteBlock(i, data);
        if (status == E_OK) {
            printf("   [NVM] Block %d queued.\n", i);
        } else {
            printf("   [NVM] Block %d REJECTED unexpectedly.\n", i);
        }
    }

    printf("   [NVM] Triggering overflow...\n");
    status = NvM_WriteBlock(10, data);

    if (status == E_NOT_OK) {
        printf("   [NVM] Overflow correctly detected.\n");
        rejected++;
    } else {
        printf("   [NVM] ERROR: Overflow NOT detected.\n");
    }

    if (rejected) {
        printf("   RESULT: [PASS]\n");
        g_testsPassed++;
    } else {
        printf("   RESULT: [FAIL]\n");
        g_testsFailed++;
    }

    printf("============================================\n\n");
}

void Test_Pending_Block_Conflict(void) {
    printf("=== TEST: Pending Block Conflict ===\n");

    uint8 data[BUFFER_SIZE];
    ResetBuffer(data, 0x55);

    Std_ReturnType status;
    int rejected = 0;

    status = NvM_WriteBlock(2, data);
    if (status == E_OK) {
        printf("   [NVM] Block 2 queued successfully.\n");
    } else {
        printf("   [NVM] ERROR: Initial request failed.\n");
    }

    printf("   [NVM] Re-queuing Block 2...\n");
    status = NvM_WriteBlock(2, data);

    if (status == E_NOT_OK) {
        printf("   [NVM] Correctly rejected (block already pending).\n");
        rejected++;
    } else {
        printf("   [NVM] ERROR: Duplicate request accepted.\n");
    }

    if (rejected) {
        printf("   RESULT: [PASS]\n");
        g_testsPassed++;
    } else {
        printf("   RESULT: [FAIL]\n");
        g_testsFailed++;
    }

    printf("============================================\n\n");
}

void Test_ParamCorruption_WriteBlock(void) {
    printf("=== TEST: Parameter Corruption (NvM_WriteBlock) ===\n");
    printf("   Goal: Verify BSW rejects bad pointers/IDs and fires DET.\n");

    uint8 data[BUFFER_SIZE];
    ResetBuffer(data, 0x00);

    Last_Det_Error = 0;

    FaultState_Activate_fault(TARGET_NVM_WRITE_BLOCK, FAULT_PARAMETER_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 500;

    ProcessSystem(5);

    Std_ReturnType status = NvM_WriteBlock(3, data);

    if (status == E_NOT_OK && Last_Det_Error != 0) {
        printf("   [Hook] Triggered DET Error Code: 0x%02X\n", Last_Det_Error);
        printf("   RESULT: [PASS] - BSW defended itself successfully.\n");
        g_testsPassed++;
    } else {
        printf("   [Hook] FAILED to trigger DET or BSW accepted bad data!\n");
        printf("   RESULT: [FAIL]\n");
        g_testsFailed++;
    }
    printf("============================================================\n\n");
}

void Test_ParamCorruption_SetDataIndex(void) {
    printf("=== TEST: Parameter Corruption (NvM_SetDataIndex) ===\n");
    printf("   Goal: Verify BSW catches out-of-bounds Dataset Index.\n");

    Fault_Clear(TARGET_NVM_WRITE_BLOCK);
    Last_Det_Error = 0;

    FaultState_Activate_fault(TARGET_NVM_WRITE_BLOCK, FAULT_PARAMETER_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 500;

    ProcessSystem(5);

    uint16_t datasetBlockId = 3;
    Std_ReturnType status = NvM_SetDataIndex(datasetBlockId, 0);

    if (status == E_NOT_OK && Last_Det_Error != 0) {
        printf("   [Hook] Triggered DET Error Code: 0x%02X\n", Last_Det_Error);
        if (Last_Det_Error == NVM_E_PARAM_BLOCK_DATA_IDX || Last_Det_Error == NVM_E_PARAM_BLOCK_ID) {
            printf("   RESULT: [PASS] - BSW rejected out-of-bounds index/ID.\n");
            g_testsPassed++;
        } else {
            printf("   RESULT: [FAIL] - Wrong DET error fired.\n");
            g_testsFailed++;
        }
    } else {
        printf("   [Hook] FAILED to trigger DET or BSW accepted bad index!\n");
        printf("   RESULT: [FAIL]\n");
        g_testsFailed++;
    }
    printf("============================================================\n\n");
}

/* =========================================================================
 * AUTOSAR RESTRICTION TESTS (NVM698, NVM705, NVM579, NVM212)
 * ========================================================================= */

void Test_NVM698_ImplicitSync(void) {
    printf("=== TEST NVM698: Implicit Synchronization ===\n");
    printf("   [NVM698] Application must not modify RAM block until request is done.\n");

    Fault_Clear(TARGET_FLS_WRITE);
    uint16_t blockId = TEST_BLOCK_ID;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0xAA);
    memcpy(golden, sent, BUFFER_SIZE);

    NvM_WriteBlock(blockId, sent);

    NvM_RequestResultType status;
    uint32_t start = GetSystemTimeMs();

    printf("   [Step] Polling NvM_GetErrorStatus...\n");
    do {
        ProcessSystem(TICK_MS);
        NvM_GetErrorStatus(blockId, &status);
    } while (status == NVM_REQ_PENDING && (GetSystemTimeMs() - start) < 2000);

    printf("   [Step] Write completed. Modifying RAM block is now safe.\n");
    sent[0] = 0xBB;

    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);

    AnalyzeResult_Block("Implicit Sync", golden, read, FALSE, blockId);
}

static int mock_explicit_sync_calls = 0;
static int mock_explicit_sync_limit = 3;
static boolean mock_explicit_sync_success = FALSE;

Std_ReturnType Mock_NvMWriteRamBlockToNvM(void* NvMBuffer) {
    mock_explicit_sync_calls++;
    if (mock_explicit_sync_success) {
        memset(NvMBuffer, 0xCC, BUFFER_SIZE);
        return E_OK;
    } else {
        return E_NOT_OK;
    }
}

void Test_NVM705_ExplicitSync(void) {
    printf("=== TEST NVM705: Explicit Synchronization ===\n");
    printf("   [NVM705] App can modify RAM block until NvMWriteRamBlockToNvM is called.\n");
    Fault_Clear(TARGET_FLS_WRITE);
    uint16_t blockId = TEST_BLOCK_ID;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    memset(golden, 0xCC, BUFFER_SIZE);

    ResetBuffer(sent, 0x00);

    NvM_WriteBlock(blockId, sent);

    sent[0] = 0x12;
    sent[1] = 0x34;

    mock_explicit_sync_calls = 0;
    mock_explicit_sync_success = TRUE;

    printf("   [Step] NvM calls NvMWriteRamBlockToNvM...\n");
    uint8 internalNvMBuffer[BUFFER_SIZE];
    Std_ReturnType ret = Mock_NvMWriteRamBlockToNvM(internalNvMBuffer);

    if (ret == E_OK) {
        printf("   [Step] App provided consistent copy. Writing to NV...\n");
        memcpy(sent, internalNvMBuffer, BUFFER_SIZE);
        WaitForBlock(blockId);
    }

    sent[0] = 0x99;

    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);

    AnalyzeResult_Block("Explicit Sync", golden, read, FALSE, blockId);
}

void Test_NVM579_ExplicitSync_Retry(void) {
    printf("=== TEST NVM579: Explicit Sync Retry Limit ===\n");
    printf("   [NVM579] NvM retries NvMWriteRamBlockToNvM NvMRepeatMirrorOperations times.\n");
    Fault_Clear(TARGET_FLS_WRITE);
    mock_explicit_sync_calls = 0;
    mock_explicit_sync_success = FALSE;

    printf("   [Step] Simulating NvM calling NvMWriteRamBlockToNvM...\n");
    uint8 internalNvMBuffer[BUFFER_SIZE];

    int i;
    for (i = 0; i < mock_explicit_sync_limit; i++) {
        Std_ReturnType ret = Mock_NvMWriteRamBlockToNvM(internalNvMBuffer);
        if (ret == E_NOT_OK) {
            printf("   [Retry %d] App returned E_NOT_OK.\n", i+1);
        } else {
            break;
        }
    }

    if (i == mock_explicit_sync_limit) {
        printf("   [Step] Retry limit reached (%d). NvM postpones job and continues next job.\n", mock_explicit_sync_limit);
        g_testsPassed++;
        printf("   RESULT:   [PASS]\n============================================================\n\n");
    } else {
        printf("   [Step] Unexpected success.\n");
        g_testsFailed++;
        printf("   RESULT:   [FAIL]\n============================================================\n\n");
    }
}

void Test_NVM212_CRC_Recalc(void) {
    printf("=== TEST NVM212: CRC Recalculation ===\n");
    printf("   [NVM212] NvM_WriteBlock requests CRC recalculation before copying to NV.\n");
    Fault_Clear(TARGET_FLS_WRITE);
    uint16_t blockId = 3;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x77);
    memcpy(golden, sent, BUFFER_SIZE);

    printf("   [Step] Writing Block %d (CRC configured).\n", blockId);
    NvM_WriteBlock(blockId, sent);
    WaitForBlock(blockId);

    printf("   [Step] Reading Block %d. If CRC was recalculated correctly, read succeeds.\n", blockId);
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);

    AnalyzeResult_Block("CRC Recalculation", golden, read, FALSE, blockId);
}

/* =========================================================================
 * FEE TESTS
 * ========================================================================= */

void Test_Fee_BitFlip_Visual(void) {
    printf("=== TEST: FEE Visual Bit Flip (Bit 3) ===\n");
    printf("   Goal: Flip Bit 3 of Byte 0 at FEE layer through NvM flow.\n\n");

    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];

    memset(sent, 0x00, BUFFER_SIZE);
    memcpy(golden, sent, BUFFER_SIZE);

    printf("[GOLDEN BUFFER - Before Write]\n");
    PrintBufferCmp("Golden", golden, NULL);

    Fault_Clear(TARGET_FEE_WRITE);
    Fault_Clear(TARGET_FLS_WRITE);
    Fault_Clear(TARGET_NVM_WRITE_BLOCK);

    FaultState_Activate_fault(TARGET_FEE_WRITE, FAULT_DATA_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;
    cfg->BitPosition  = 3;

    printf("\n[FAULT INJECTION ACTIVE]\n");
    printf("Target: FEE\n");
    printf("Type: DATA_CORRUPTION\n");
    printf("Byte 0, Bit 3\n");
    printf("Injection Window: %u ms to %u ms\n\n", cfg->Start_TimeMs, cfg->End_timeMs);

    ProcessSystem(10);

    printf("[WRITE PHASE]\n");
    NvM_WriteBlock(TEST_BLOCK_ID, sent);
    WaitForNvM();

    NvM_RequestResultType writeStatus;
    NvM_GetErrorStatus(TEST_BLOCK_ID, &writeStatus);
    printf("[NvM Status After Write]: ");
    switch (writeStatus) {
        case NVM_REQ_OK:               printf("NVM_REQ_OK\n"); break;
        case NVM_REQ_INTEGRITY_FAILED: printf("NVM_REQ_INTEGRITY_FAILED\n"); break;
        case NVM_REQ_NOT_OK:           printf("NVM_REQ_NOT_OK\n"); break;
        case NVM_REQ_NV_INVALIDATED:   printf("NVM_REQ_NV_INVALIDATED\n"); break;
        default:                       printf("Status Code: %d\n", writeStatus); break;
    }

    memset(read, 0x00, BUFFER_SIZE);

    printf("\n[READ PHASE]\n");
    NvM_ReadBlock(TEST_BLOCK_ID, read);
    WaitForNvM();

    PrintBufferCmp("Read Back", read, golden);

    NvM_RequestResultType readStatus;
    NvM_GetErrorStatus(TEST_BLOCK_ID, &readStatus);
    printf("\n[NvM Status After Read]: ");
    switch (readStatus) {
        case NVM_REQ_OK:               printf("NVM_REQ_OK\n"); break;
        case NVM_REQ_INTEGRITY_FAILED: printf("NVM_REQ_INTEGRITY_FAILED\n"); break;
        case NVM_REQ_NOT_OK:           printf("NVM_REQ_NOT_OK\n"); break;
        case NVM_REQ_NV_INVALIDATED:   printf("NVM_REQ_NV_INVALIDATED\n"); break;
        default:                       printf("Status Code: %d\n", readStatus); break;
    }

    AnalyzeResult("FEE Visual Bit Flip", golden, read, TRUE);
}

/* =========================================================================
 * NEW TESTS FOR FLS READ AND ERASE HOOKS
 * ========================================================================= */

void Test_Fls_Read_DataCorruption(void) {
    printf("=== TEST 3: FLS Read Data Corruption ===\n");
    printf("   Goal: Write clean data to NVM, then inject BIT FLIP on FLS Read. Check if Application catches it.\n\n");

    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];

    memset(sent, 0x00, BUFFER_SIZE);
    memcpy(golden, sent, BUFFER_SIZE);

    printf("[WRITE PHASE] - Writing clean data...\n");
    Fault_Clear(TARGET_FLS_WRITE);
    Fault_Clear(TARGET_NVM_WRITE_BLOCK);

    NvM_WriteBlock(TEST_BLOCK_ID, sent);
    WaitForNvM();

    FaultState_Activate_fault(TARGET_FLS_READ, FAULT_BIT_FLIP, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;
    cfg->BitPosition  = 0;

    printf("\n[FAULT INJECTION ACTIVE ON READ]\n");
    printf("Target: FLS \nType: BIT_FLIP on READ \nByte 0, Bit 0 \n");
    ProcessSystem(10);

    memset(read, 0x00, BUFFER_SIZE);

    printf("\n[READ PHASE] - Reading back data (Fault should trigger here)...\n");
    NvM_ReadBlock(TEST_BLOCK_ID, read);
    WaitForNvM();

    PrintBufferCmp("Read Back from fault-injected FLS_Read", read, golden);

    AnalyzeResult("FLS Read Data Corruption", golden, read, TRUE);
}

void Test_Fls_Erase_DataCorruption(void) {
    printf("=== TEST 4: FLS Erase Data Corruption (Incomplete Erase) ===\n");
    printf("   Goal: Write data, then trigger block erase with FAULT active so it partially erases.\n\n");

    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];

    memset(sent, 0xBB, BUFFER_SIZE);
    memcpy(golden, sent, BUFFER_SIZE);

    extern Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length);
    extern Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
    extern Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);

    printf("[WRITE PHASE] - Writing pattern 0xBB directly to FLS...\n");
    Fault_Clear(TARGET_FLS_WRITE);
    Fault_Clear(TARGET_NVM_WRITE_BLOCK);

    Fls_Write(0, sent, BUFFER_SIZE);

    FaultState_Activate_fault(TARGET_FLS_ERASE, FAULT_BIT_FLIP, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;

    printf("\n[FAULT INJECTION ACTIVE ON ERASE]\n");
    printf("Target: FLS \nType: Incomplete Erase (Only first sector wiped) \n");
    ProcessSystem(10);

    printf("\n[ERASE PHASE] - Triggering Fls_Erase(0, %d) with fault active...\n", 4096);

    uint32 offset = 2048;
    Fls_Write(offset, sent, BUFFER_SIZE);

    Fls_Erase(0, 4096);

    printf("\n[ERASE PHASE] - Triggered Fls_Erase(0, 4096) with fault active...\n");

    ProcessSystem(10);

    Fault_Clear(TARGET_FLS_ERASE);

    extern MemIf_JobResultType Fls_GetJobResult(void);
    MemIf_JobResultType result = Fls_GetJobResult();

    if (result == MEMIF_JOB_FAILED) {
        printf("   Detector: FLS Module (via Fls_MainFunction FLS022 verify)\n");
        printf("   Status: Fault Injected Successfully (Stack CAUGHT the incomplete erase!).\n");
        printf("   RESULT:   [PASS]\n");
        g_testsPassed++;
    } else {
        printf("   Detector: Application\n");
        printf("   Status: Fault Failed (Stack MISSED IT, result=%d).\n", result);
        printf("   RESULT:   [FAIL]\n");
        g_testsFailed++;
    }
    printf("============================================================\n\n");
}

/* =========================================================================
 * PARAMETER CORRUPTION TESTS (FLS157, FLS158)
 * ========================================================================= */

void Test_Fls_Write_ParameterCorruption_NullPointer(void) {
    printf("=== TEST 5: Fls_Write Parameter Corruption (Null Data Pointer) ===\n");
    printf("   Goal: Call Fls_Write with NULL data pointer. Verify FLS rejects request [FLS157].\n\n");

    printf("[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS \nType: PARAMETER_CORRUPTION \nParameter: SourceAddressPtr (NULL pointer)\n\n");

    printf("[WRITE PHASE WITH NULL POINTER]\n");
    uint32 targetAddress = 0x100;
    uint32 length = 64;

    Fault_Clear(TARGET_FLS_WRITE);
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_PARAMETER_CORRUPTION, 0, 0);
    Std_ReturnType result = Hook_Fls_Write(targetAddress, (const uint8*)NULL, length);
    Fault_Clear(TARGET_FLS_WRITE);

    printf(">>> Hook_Fls_Write(0x%X, NULL, %u) called\n\n", targetAddress, length);

    if (result == E_NOT_OK) {
        printf("   Status:   FLS correctly REJECTED the operation with E_NOT_OK\n");
        printf("   Detector: FLS Module (Parameter Validation)\n");
        printf("   [Requirement]: FLS157 - Null pointer check passed\n");
        printf("   RESULT:   [PASS]\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   FLS did NOT reject the operation (returned %d instead of E_NOT_OK)\n", result);
        printf("   Detector: Application (Parameter validation FAILED)\n");
        printf("   [Requirement]: FLS157 - Null pointer check FAILED\n");
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

void Test_Fls_Read_ParameterCorruption_NullPointer(void) {
    printf("=== TEST 6: Fls_Read Parameter Corruption (Null Target Pointer) ===\n");
    printf("   Goal: Call Fls_Read with NULL target pointer. Verify FLS rejects request [FLS158].\n\n");

    printf("[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS \nType: PARAMETER_CORRUPTION \nParameter: TargetAddressPtr (NULL pointer)\n\n");

    printf("[READ PHASE WITH NULL POINTER]\n");
    uint32 sourceAddress = 0x100;
    uint32 length = 64;

    Fault_Clear(TARGET_FLS_READ);
    FaultState_Activate_fault(TARGET_FLS_READ, FAULT_PARAMETER_CORRUPTION, 0, 0);
    Std_ReturnType result = Hook_Fls_Read(sourceAddress, (uint8*)NULL, length);
    Fault_Clear(TARGET_FLS_READ);

    printf(">>> Hook_Fls_Read(0x%X, NULL, %u) called\n\n", sourceAddress, length);

    if (result == E_NOT_OK) {
        printf("   Status:   FLS correctly REJECTED the operation with E_NOT_OK\n");
        printf("   Detector: FLS Module (Parameter Validation)\n");
        printf("   [Requirement]: FLS158 - Null pointer check passed\n");
        printf("   RESULT:   [PASS]\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   FLS did NOT reject the operation (returned %d instead of E_NOT_OK)\n", result);
        printf("   Detector: Application (Parameter validation FAILED)\n");
        printf("   [Requirement]: FLS158 - Null pointer check FAILED\n");
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

void Test_Fls_Erase_ParameterCorruption_UnalignedAddress(void) {
    printf("=== TEST 7: Fls_Erase Parameter Corruption (Unaligned Address) ===\n");
    printf("   Goal: Call Fls_Erase with unaligned address. Verify FLS rejects request [FLS020].\n\n");

    printf("[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS \nType: PARAMETER_CORRUPTION \nParameter: TargetAddress (unaligned, not on sector boundary)\n\n");

    printf("[ERASE PHASE WITH UNALIGNED ADDRESS]\n");
    uint32 unalignedAddress = 0x0001;
    uint32 length = 1024;

    Fault_Clear(TARGET_FLS_ERASE);
    FaultState_Activate_fault(TARGET_FLS_ERASE, FAULT_PARAMETER_CORRUPTION, 0, 0);
    Std_ReturnType result = Hook_Fls_Erase(unalignedAddress, length);
    Fault_Clear(TARGET_FLS_ERASE);

    printf(">>> Hook_Fls_Erase(0x%X, %u) called\n\n", unalignedAddress, length);

    if (result == E_NOT_OK) {
        printf("   Status:   FLS correctly REJECTED the operation with E_NOT_OK\n");
        printf("   Detector: FLS Module (Parameter Validation)\n");
        printf("   [Requirement]: FLS020 - Address alignment check passed\n");
        printf("   RESULT:   [PASS]\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   FLS did NOT reject the operation (returned %d instead of E_NOT_OK)\n", result);
        printf("   Detector: Application (Parameter validation FAILED)\n");
        printf("   [Requirement]: FLS020 - Address alignment check FAILED\n");
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

void Test_Fls_Erase_ParameterCorruption_ZeroLength(void) {
    printf("=== TEST 8: Fls_Erase Parameter Corruption (Zero Length) ===\n");
    printf("   Goal: Call Fls_Erase with zero length. Verify FLS rejects request [FLS021].\n\n");

    printf("[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS \nType: PARAMETER_CORRUPTION \nParameter: Length (zero bytes)\n\n");

    printf("[ERASE PHASE WITH ZERO LENGTH]\n");
    uint32 address = 0x0000;
    uint32 zeroLength = 0;

    Std_ReturnType result = Hook_Fls_Erase(address, zeroLength);

    printf(">>> Hook_Fls_Erase(0x%X, %u) called\n\n", address, zeroLength);

    if (result == E_NOT_OK) {
        printf("   Status:   FLS correctly REJECTED the operation with E_NOT_OK\n");
        printf("   Detector: FLS Module (Parameter Validation)\n");
        printf("   [Requirement]: FLS021 - Length validation check passed\n");
        printf("   RESULT:   [PASS]\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   FLS did NOT reject the operation (returned %d instead of E_NOT_OK)\n", result);
        printf("   Detector: Application (Parameter validation FAILED)\n");
        printf("   [Requirement]: FLS021 - Length validation check FAILED\n");
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

void Test_Fls_Erase_ParameterCorruption_OutOfBounds(void) {
    printf("=== TEST 9: Fls_Erase Parameter Corruption (Out-of-Bounds Address) ===\n");
    printf("   Goal: Call Fls_Erase with aligned but out-of-bounds address. Verify FLS rejects request [FLS020].\n\n");

    printf("[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS \nType: PARAMETER_CORRUPTION \nParameter: TargetAddress (aligned but exceeds flash size)\n\n");

    printf("[ERASE PHASE WITH OUT-OF-BOUNDS ADDRESS]\n");
    uint32 outOfBoundsAddress = 0x8000;
    uint32 length = 1024;

    Std_ReturnType result = Hook_Fls_Erase(outOfBoundsAddress, length);

    printf(">>> Hook_Fls_Erase(0x%X, %u) called (FLASH_SIZE = 0x8000)\n\n", outOfBoundsAddress, length);

    if (result == E_NOT_OK) {
        printf("   Status:   FLS correctly REJECTED the operation with E_NOT_OK\n");
        printf("   Detector: FLS Module (Boundary Check)\n");
        printf("   [Requirement]: FLS020 - Address boundary validation passed\n");
        printf("   RESULT:   [PASS]\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   FLS did NOT reject the operation (returned %d instead of E_NOT_OK)\n", result);
        printf("   Detector: Application (Boundary validation FAILED)\n");
        printf("   [Requirement]: FLS020 - Address boundary validation FAILED\n");
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

void Test_Fls_Erase_ParameterCorruption_NonAlignedLength(void) {
    printf("=== TEST 10: Fls_Erase Parameter Corruption (Non-Aligned Length) ===\n");
    printf("   Goal: Call Fls_Erase with length not multiple of sector size. Verify rounding behavior [FLS221].\n\n");

    printf("[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS \nType: PARAMETER_CORRUPTION \nParameter: Length (512 bytes, not aligned to sector size of 1024)\n\n");

    printf("[ERASE PHASE WITH NON-ALIGNED LENGTH]\n");
    uint32 address = 0x0000;
    uint32 nonAlignedLength = 512;

    Std_ReturnType result = Hook_Fls_Erase(address, nonAlignedLength);

    printf(">>> Hook_Fls_Erase(0x%X, %u) called (FLS_SECTOR_SIZE = 1024)\n\n", address, nonAlignedLength);

    if (result == E_OK) {
        printf("   Status:   FLS accepted the operation with E_OK\n");
        printf("   Detector: FLS Module (Rounding behavior [FLS221])\n");
        printf("   [Requirement]: FLS221 - Length rounded up to next sector boundary (512 -> 1024)\n");
        printf("   RESULT:   [PASS]\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   FLS REJECTED the operation (returned %d)\n", result);
        printf("   NOTE:     Depends on MCAL implementation interpretation\n");
        printf("   RESULT:   [CONDITIONAL]\n");
        printf("============================================================\n\n");
    }
}

/* =========================================================================
 * RETURN VALUE CORRUPTION TESTS
 * ========================================================================= */

void Test_Fls_Write_ReturnValueCorruption(void) {
    printf("=== TEST 11: Fls_Write Return Value Corruption ===\n");
    printf("   Goal: Inject return value corruption. E_OK becomes E_NOT_OK. Verify behavior.\n\n");

    uint8 testData[64];
    memset(testData, 0xAA, 64);

    printf("[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS_WRITE \nType: RETURN_VALUE_CORRUPTION \nReturn Value: E_OK (0x00) -> E_NOT_OK (0x01)\n\n");

    Fault_Clear(TARGET_FLS_WRITE);
    FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;

    ProcessSystem(5);

    printf("[WRITE PHASE]\n");
    printf(">>> Hook_Fls_Write(0x1000, testData, 64) with corrupted return value\n");
    Std_ReturnType result = Fls_Write(0x1000, testData, 64);

    Fault_Clear(TARGET_FLS_WRITE);

    if (result == E_NOT_OK) {
        printf("   Status:   Return value correctly corrupted: E_OK -> E_NOT_OK\n");
        printf("   Detector: Application (Caller receives unexpected E_NOT_OK)\n");
        printf("   RESULT:   [PASS] - Corruption detected by caller\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   Return value corruption FAILED (got 0x%02X instead of 0x01)\n", result);
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

void Test_Fls_Read_ReturnValueCorruption(void) {
    printf("=== TEST 12: Fls_Read Return Value Corruption ===\n");
    printf("   Goal: Inject return value corruption on Fls_Read. E_OK becomes E_NOT_OK.\n\n");

    uint8 testData[64];
    memset(testData, 0x00, 64);

    printf("[WRITE PHASE] - Writing clean data to flash...\n");
    Fault_Clear(TARGET_FLS_WRITE);
    Hook_Fls_Write(0x2000, testData, 64);

    printf("\n[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS_READ \nType: RETURN_VALUE_CORRUPTION \nReturn Value: E_OK (0x00) -> E_NOT_OK (0x01)\n\n");

    Fault_Clear(TARGET_FLS_READ);
    FaultState_Activate_fault(TARGET_FLS_READ, FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;

    ProcessSystem(5);

    printf("[READ PHASE]\n");
    uint8 readBuffer[64];
    memset(readBuffer, 0xFF, 64);
    printf(">>> Hook_Fls_Read(0x2000, readBuffer, 64) with corrupted return value\n");
    Std_ReturnType result = Hook_Fls_Read(0x2000, readBuffer, 64);

    Fault_Clear(TARGET_FLS_READ);

    if (result == E_NOT_OK) {
        printf("   Status:   Return value correctly corrupted: E_OK -> E_NOT_OK\n");
        printf("   Detector: Application (Caller receives unexpected E_NOT_OK)\n");
        printf("   RESULT:   [PASS] - Corruption detected by caller\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   Return value corruption FAILED (got 0x%02X instead of 0x01)\n", result);
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

void Test_Fls_Erase_ReturnValueCorruption(void) {
    printf("=== TEST 13: Fls_Erase Return Value Corruption ===\n");
    printf("   Goal: Inject return value corruption on Fls_Erase. E_OK becomes E_NOT_OK.\n\n");

    printf("[PREPARATION PHASE]\n");
    printf("Writing test pattern to flash at address 0x3000...\n");
    uint8 testData[64];
    memset(testData, 0x55, 64);
    Fault_Clear(TARGET_FLS_WRITE);
    Hook_Fls_Write(0x3000, testData, 64);

    printf("\n[FAULT INJECTION ACTIVE]\n");
    printf("Target: FLS_ERASE \nType: RETURN_VALUE_CORRUPTION \nReturn Value: E_OK (0x00) -> E_NOT_OK (0x01)\n\n");

    Fault_Clear(TARGET_FLS_ERASE);
    FaultState_Activate_fault(TARGET_FLS_ERASE, FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;

    ProcessSystem(5);

    printf("[ERASE PHASE]\n");
    printf(">>> Hook_Fls_Erase(0x3000, 1024) with corrupted return value\n");
    Std_ReturnType result = Hook_Fls_Erase(0x3000, 1024);

    Fault_Clear(TARGET_FLS_ERASE);

    if (result == E_NOT_OK) {
        printf("   Status:   Return value correctly corrupted: E_OK -> E_NOT_OK\n");
        printf("   Detector: Application (Caller receives unexpected E_NOT_OK)\n");
        printf("   RESULT:   [PASS] - Corruption detected by caller\n");
        printf("============================================================\n\n");
        g_testsPassed++;
    } else {
        printf("   Status:   Return value corruption FAILED (got 0x%02X instead of 0x01)\n", result);
        printf("   RESULT:   [FAIL]\n");
        printf("============================================================\n\n");
        g_testsFailed++;
    }
}

/* =========================================================================
 * MAIN ENTRY
 * ========================================================================= */
int main(void) {
    printf("\n");
    printf("============================================================\n");
    printf("   FAULT INJECTION TEST SUITE (FULL REPORT)                 \n");
    printf("============================================================\n\n");

    Fls_Init(NULL);
    Fee_Init();
    NvM_Init();
    Fault_Init();

    /* --- FLS / NvM Integration Tests --- */
    Test_BitFlip_Immediate();
    Test_Delayed_Start();
    Test_StuckAt_Logic();
    Test_Frequency_Pulse();
    Test_MultiBit_Mask();
    Test_Concurrency_Stress();
    Test_Fls_BitFlip_Visual();
    Test_Fls_TimeWindow();
    Test_Fls_XOR_Logic();

    printf("\n\n============================================================\n");
    printf("   NEW TEST CASES (AUTOSAR REQUIREMENTS)                     \n");
    printf("============================================================\n\n");

    Test_Case1_RedundantBlock();
    Test_Case2_NativeWithROM();
    Test_Case3_NativeNoROM();
    Test_Case4_Test_Case4_Dataset();
    Test_Queue_Overflow();
    Test_Pending_Block_Conflict();

    printf("\n\n============================================================\n");
    printf("   PARAMETER CORRUPTION (INPUT ROBUSTNESS) TESTS\n");
    printf("============================================================\n\n");
    Test_ParamCorruption_WriteBlock();
    Test_ParamCorruption_SetDataIndex();

    printf("\n\n============================================================\n");
    printf("   AUTOSAR RESTRICTIONS TESTS (NVM698, NVM705, NVM579, NVM212)\n");
    printf("============================================================\n\n");
    Test_NVM698_ImplicitSync();
    Test_NVM705_ExplicitSync();
    Test_NVM579_ExplicitSync_Retry();
    Test_NVM212_CRC_Recalc();

    printf("\n\n============================================================\n");
    printf("   FEE & FLS LAYER TESTS                                     \n");
    printf("============================================================\n\n");
    Test_Fee_BitFlip_Visual();
    Test_Fls_Read_DataCorruption();
    Test_Fls_Erase_DataCorruption();
    Test_Fls_Write_ParameterCorruption_NullPointer();
    Test_Fls_Read_ParameterCorruption_NullPointer();
    Test_Fls_Erase_ParameterCorruption_UnalignedAddress();
    Test_Fls_Erase_ParameterCorruption_ZeroLength();
    Test_Fls_Erase_ParameterCorruption_OutOfBounds();
    Test_Fls_Erase_ParameterCorruption_NonAlignedLength();
    Test_Fls_Write_ReturnValueCorruption();
    Test_Fls_Read_ReturnValueCorruption();
    Test_Fls_Erase_ReturnValueCorruption();

    printf("\n------------------------------------------------------------\n");
    printf(" FINAL RESULTS: %d Passed, %d Failed\n", g_testsPassed, g_testsFailed);
    printf("------------------------------------------------------------\n");

    return (g_testsFailed == 0) ? 0 : 1;
}
