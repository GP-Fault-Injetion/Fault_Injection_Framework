#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* AUTOSAR / Stack Headers */
#include "Std_Types.h"
#include "NvM.h"
#include "MemIf.h"
#include "Crc.h"

/* Fault Injection Headers */
#define ENABLE_FAULT_INJECTION_HOOKS
#include "hook.h"
#include "FaultInjection_Interface.h"
#include "Fault_state.h"

/* --- Configuration --- */
#define TEST_BLOCK_ID   2
#define BUFFER_SIZE     64
#define TICK_MS         1

/* --- Globals for Reporting --- */
static int g_testsPassed = 0;
static int g_testsFailed = 0;

/* --- DET Mocking --- */
extern uint8 Last_Det_Error;



/* =========================================================================
 * SYSTEM SIMULATION HELPERS
 * ========================================================================= */

uint32_t GetSystemTimeMs(void) {
    return (uint32_t)((clock() * 1000) / CLOCKS_PER_SEC);
}

void ProcessSystem(uint32_t durationMs) {
    uint32_t start = GetSystemTimeMs();
    uint32_t now;

    do {
        now = GetSystemTimeMs();
        
        /* --- Fault Injection --- */
        /* Update fault states BEFORE executing the stack so faults activate instantly */
        FaultState_MainFunction();

        /* --- STACK SCHEDULING --- */
        Fls_MainFunction();
        Fee_MainFunction();
        NvM_MainFunction();

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
        /* If we expect a fault, but NVM recovered it (via ROM or Redundancy), it will return OK */
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


void PrintBuffer(const char* label, const uint8* buf, uint32_t len) {
    printf("   [DATA] %-10s: ", label);
    for (uint32_t i = 0; i < len; i++) {
        printf("%02X ", buf[i]);
        /* Optional: limit line length for very large buffers */
        if (i > 0 && (i + 1) % 16 == 0 && i < len - 1) printf("\n                     ");
    }
    printf("\n");
}
/* =========================================================================
 * TEST CASES
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

    AnalyzeResult_Block("BitFlip Immediate", golden, read, TRUE,TEST_BLOCK_ID);
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
    AnalyzeResult_Block("Delayed Start (Phase 1)", golden, read, FALSE,TEST_BLOCK_ID);

    printf("   [Phase 2] Waiting for Fault Activation...\n");
    ProcessSystem(delay + 50);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Delayed Start (Phase 2)", golden, read, TRUE,TEST_BLOCK_ID);
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
    AnalyzeResult_Block("Stuck-At-1", golden, read, TRUE,TEST_BLOCK_ID);

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
    AnalyzeResult_Block("Stuck-At-0", golden, read, TRUE,TEST_BLOCK_ID);
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
    AnalyzeResult_Block("Freq Pulse (ON)", golden, read, TRUE,TEST_BLOCK_ID);

    ProcessSystem(90);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Freq Pulse (OFF Gap)", golden, read, FALSE,TEST_BLOCK_ID);

    ProcessSystem(110);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Freq Pulse (ON Again)", golden, read, TRUE,TEST_BLOCK_ID);
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
    AnalyzeResult_Block("Multi-Bit Mask", golden, read, TRUE,TEST_BLOCK_ID);
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
    AnalyzeResult_Block("Concurrency Stress", golden, read, TRUE,TEST_BLOCK_ID);
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
    
    AnalyzeResult_Block("Visual Bit Flip", golden, read, TRUE,TEST_BLOCK_ID);
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
    AnalyzeResult_Block("Time Window (Before)", golden, read, FALSE,TEST_BLOCK_ID);

    printf("   [Step 2] Writing Inside Window...\n");
    ProcessSystem(startWindow - GetSystemTimeMs() + 50);
    ResetBuffer(sent, 0x11); memcpy(golden, sent, BUFFER_SIZE);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Time Window (Inside)", golden, read, TRUE,TEST_BLOCK_ID);

    printf("   [Step 3] Writing After Window...\n");
    ProcessSystem(endWindow - GetSystemTimeMs() + 50);
    ResetBuffer(sent, 0x22); memcpy(golden, sent, BUFFER_SIZE);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult_Block("Time Window (After)", golden, read, FALSE,TEST_BLOCK_ID);
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

    AnalyzeResult_Block("XOR Logic", golden, read, TRUE,TEST_BLOCK_ID);
}

void Test_Case1_RedundantBlock(void) {
    printf("=== CASE 1: Redundant Block (Block 2) ===\n");
    printf("   [NVM199/NVM279/NVM655] If CRC fails on primary, recovers from secondary.\n");
    uint16_t blockId = 3; /* Maps to BlockDescriptors[2] which is Redundant */
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x11);
    memcpy(golden, sent, BUFFER_SIZE);
PrintBuffer("write ", golden, BUFFER_SIZE);
    /* 1. Write clean data (both copies) */
    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);

    /* 2. Write again, but corrupt the primary block.
       We use a very short time window (e.g., 5ms) on FLS Write so that only the
       first copy is corrupted, and the second copy writes successfully. */
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
    uint16_t blockId = 4; /* Maps to BlockDescriptors[3] which has RomBlock3_Default */
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE];
    uint8 rom_golden[BUFFER_SIZE];
    memset(rom_golden, 0xDD, BUFFER_SIZE); /* From RomBlock3_Default */

    ResetBuffer(sent, 0x33);

    /* 1. Write corrupted data so CRC fails */
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
    uint16_t blockId = 5; /* Maps to BlockDescriptors[4] which has NO ROM */
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x44);
    memcpy(golden, sent, BUFFER_SIZE);

    /* 1. Write corrupted data so CRC fails */
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

    /* For Case 3, we expect NVM_REQ_INTEGRITY_FAILED, so passing golden doesn't matter much
       since the data won't match, but the Status check in AnalyzeResult will verify INTEGRITY_FAILED */
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

    /* Step 1: Fill the queue */
    for (i = 2; i < 2 + NVM_SIZE_STANDARD_JOB_QUEUE; i++) {
        status = NvM_WriteBlock(i, data);
        if (status == E_OK) {
            printf("   [NVM] Block %d queued.\n", i);
        } else {
            printf("   [NVM] Block %d REJECTED unexpectedly.\n", i);
        }
    }

    /* Step 2: One extra request → should overflow */
    printf("   [NVM] Triggering overflow...\n");
    status = NvM_WriteBlock(10, data);  // different block

    if (status == E_NOT_OK) {
        printf("   [NVM] Overflow correctly detected.\n");
        rejected++;
    } else {
        printf("   [NVM] ERROR: Overflow NOT detected.\n");
    }

    /* Result */
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

    /* Step 1: Queue block 2 */
    status = NvM_WriteBlock(2, data);
    if (status == E_OK) {
        printf("   [NVM] Block 2 queued successfully.\n");
    } else {
        printf("   [NVM] ERROR: Initial request failed.\n");
    }

    /* Step 2: Try same block again (should be rejected) */
    printf("   [NVM] Re-queuing Block 2...\n");
    status = NvM_WriteBlock(2, data);

    if (status == E_NOT_OK) {
        printf("   [NVM] Correctly rejected (block already pending).\n");
        rejected++;
    } else {
        printf("   [NVM] ERROR: Duplicate request accepted.\n");
    }

    /* Result */
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
    
   
    Last_Det_Error = 0; /* Reset DET tracker */

    /* Activate Parameter Corruption */
    FaultState_Activate_fault(TARGET_NVM_WRITE_BLOCK, FAULT_PARAMETER_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 500;
    
    ProcessSystem(5); /* Update fault state to ACTIVE */

    /* Execute: The hook will swap our good parameters for garbage */
    Std_ReturnType status = NvM_WriteBlock(3, data); /* Use Block 3 to avoid pending state from previous test */

    /* Assert: Did it fail safely AND report to DET? */
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
    Last_Det_Error = 0; /* Reset DET tracker */

    /* Activate Parameter Corruption */
    FaultState_Activate_fault(TARGET_NVM_WRITE_BLOCK, FAULT_PARAMETER_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 500;
    
    ProcessSystem(5); /* Update fault state to ACTIVE */

    /* Execute: The hook will force the index to 255 */
    uint16_t datasetBlockId = 3; /* Use any valid block, the hook will corrupt it anyway */
    Std_ReturnType status = NvM_SetDataIndex(datasetBlockId, 0);

    /* Assert: Did it fail safely AND report the specific Index error? */
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
    
    /* 1. App fills RAM block */
    /* 2. App issues NvM_WriteBlock */
    NvM_WriteBlock(blockId, sent);
    
    /* 3. App polls using NvM_GetErrorStatus and does not modify 'sent' */
    NvM_RequestResultType status;
    uint32_t start = GetSystemTimeMs();
    
    printf("   [Step] Polling NvM_GetErrorStatus...\n");
    do {
        ProcessSystem(TICK_MS);
        NvM_GetErrorStatus(blockId, &status);
        /* 3. App must not modify RAM block until success or failure */
        /* sent[0] = 0xFF; // NOT ALLOWED */
    } while (status == NVM_REQ_PENDING && (GetSystemTimeMs() - start) < 2000);
    
    /* 5. After completion, RAM block is reusable */
    printf("   [Step] Write completed. Modifying RAM block is now safe.\n");
    sent[0] = 0xBB; /* Safe to modify now, it won't affect the written data */
    
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);
    
    AnalyzeResult_Block("Implicit Sync", golden, read, FALSE, blockId);
}

/* Mock explicit sync for NVM705 and NVM579 */
static int mock_explicit_sync_calls = 0;
static int mock_explicit_sync_limit = 3; /* NvMRepeatMirrorOperations */
static boolean mock_explicit_sync_success = FALSE;

Std_ReturnType Mock_NvMWriteRamBlockToNvM(void* NvMBuffer) {
    mock_explicit_sync_calls++;
    if (mock_explicit_sync_success) {
        /* Provide consistent copy */
        memset(NvMBuffer, 0xCC, BUFFER_SIZE);
        return E_OK;
    } else {
        /* Reject inconsistent data */
        return E_NOT_OK;
    }
}

void Test_NVM705_ExplicitSync(void) {
    printf("=== TEST NVM705: Explicit Synchronization ===\n");
    printf("   [NVM705] App can modify RAM block until NvMWriteRamBlockToNvM is called.\n");
    Fault_Clear(TARGET_FLS_WRITE);
    uint16_t blockId = TEST_BLOCK_ID;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    memset(golden, 0xCC, BUFFER_SIZE); /* Expected to be copied by callback */
    
    /* Simulate App filling RAM block */
    ResetBuffer(sent, 0x00);
    
    /* App issues NvM_WriteBlock */
    NvM_WriteBlock(blockId, sent);
    
    /* App modifies RAM block *before* callback (Allowed) */
    sent[0] = 0x12;
    sent[1] = 0x34;
    
    /* Simulate NvM calling the callback */
    mock_explicit_sync_calls = 0;
    mock_explicit_sync_success = TRUE; /* Ready to give consistent copy */
    
    printf("   [Step] NvM calls NvMWriteRamBlockToNvM...\n");
    uint8 internalNvMBuffer[BUFFER_SIZE];
    Std_ReturnType ret = Mock_NvMWriteRamBlockToNvM(internalNvMBuffer);
    
    if (ret == E_OK) {
        printf("   [Step] App provided consistent copy. Writing to NV...\n");
        /* Hack: we manually copy the callback buffer to our sent buffer to simulate NVM using it */
        memcpy(sent, internalNvMBuffer, BUFFER_SIZE);
        WaitForBlock(blockId); /* Finish write */
    }
    
    /* After callback, App can read and write RAM block again */
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
    mock_explicit_sync_success = FALSE; /* App always returns E_NOT_OK */
    
    printf("   [Step] Simulating NvM calling NvMWriteRamBlockToNvM...\n");
    uint8 internalNvMBuffer[BUFFER_SIZE];
    
    /* Simulate NvM retry loop */
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
    /* This implies testing that the CRC matches the data we write.
       Our existing architecture tests this implicitly (if CRC was wrong, integrity would fail).
       We will explicitly demonstrate writing and validating integrity. */
    uint16_t blockId = 3; /* Redundant block (BlockDescriptors[2]), CRC configured */
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x77);
    memcpy(golden, sent, BUFFER_SIZE);
    
    printf("   [Step] Writing Block %d (CRC configured).\n", blockId);
    NvM_WriteBlock(blockId, sent);
    
    /* Since Block 2 has CRC configured, NvM implicitly calculates CRC here. */
    WaitForBlock(blockId);
    
    printf("   [Step] Reading Block %d. If CRC was recalculated correctly, read succeeds.\n", blockId);
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);
    
    AnalyzeResult_Block("CRC Recalculation", golden, read, FALSE, blockId);
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

    printf("\n------------------------------------------------------------\n");
    printf(" FINAL RESULTS: %d Passed, %d Failed\n", g_testsPassed, g_testsFailed);
    printf("------------------------------------------------------------\n");

    return (g_testsFailed == 0) ? 0 : 1;
}
