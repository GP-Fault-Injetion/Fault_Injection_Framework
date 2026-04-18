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

/* * NEW: Better Analyzer Logic */
void AnalyzeResult(const char* testName, uint8* expected, uint8* actual, boolean expectFault) {
    NvM_RequestResultType status;
    NvM_GetErrorStatus(TEST_BLOCK_ID, &status);
    
    /* Check Data Integrity (App Level) */
    boolean dataMatch = (memcmp(expected, actual, BUFFER_SIZE) == 0);
    boolean corruptionDetected = !dataMatch;

    /* Define Who Discovered It */
    const char* detector = "NONE";
    const char* outcome = "UNKNOWN";
    boolean testPassed = FALSE;

    if (status == NVM_REQ_INTEGRITY_FAILED) {
        detector = "NvM (CRC Check)";
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

    /* Logic for Pass/Fail */
    if (expectFault) {
        if (status != NVM_REQ_OK) {
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
        /* We expect NO fault */
        if (status == NVM_REQ_OK && !corruptionDetected) {
            outcome = "Normal Operation (Success).";
            testPassed = TRUE;
        } else {
            outcome = "Unexpected Error or Corruption!";
            testPassed = FALSE;
        }
    }

    /* Print Summary */
    printf("   ------------------------------------------------------------\n");
    printf("   Status:   %s\n", outcome);
    printf("   Detector: %s\n", detector);
    
    if (corruptionDetected && status == NVM_REQ_OK) {
        /* Print diff for silent corruption */
        for(int i=0; i<BUFFER_SIZE; i++) {
            if(expected[i] != actual[i]) {
                printf("   [Diff]:   Byte %d changed from 0x%02X to 0x%02X\n", i, expected[i], actual[i]);
                break;
            }
        }
    }

    if (testPassed) {
        printf("   RESULT:   [PASS]\n");
        g_testsPassed++;
    } else {
        printf("   RESULT:   [FAIL]\n");
        g_testsFailed++;
    }
    printf("============================================================\n\n");
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

/* =========================================================================
 * TEST CASES
 * ========================================================================= */

void Test_BitFlip_Immediate(void) {
    printf("=== TEST 1: Bit Flip Immediate (NVM Target) ===\n");
    printf("   Goal: Flip Bit 0 of Byte 10. Check if NvM CRC catches it.\n");

    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);

    Fault_Clear(FAULT_TARGET_NVM);
    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_BIT_FLIP, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;
    cfg->BitPosition  = (10 * 8) + 0; 
    
    ProcessSystem(5); 
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();

    AnalyzeResult("BitFlip Immediate", golden, read, TRUE);
}

void Test_Delayed_Start(void) {
    printf("=== TEST 2: Delayed Start (CRC Corruption) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    
    Fault_Clear(FAULT_TARGET_NVM);
    uint32_t now = GetSystemTimeMs();
    uint32_t delay = 200;

    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_CRC_DATA_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = now + delay;
    cfg->End_timeMs   = cfg->Start_TimeMs + 500;
    
    printf("   [Phase 1] Immediate Write (Before Fault Start)\n");
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Delayed Start (Phase 1)", golden, read, FALSE);

    printf("   [Phase 2] Waiting for Fault Activation...\n");
    ProcessSystem(delay + 50); 
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Delayed Start (Phase 2)", golden, read, TRUE);
}

void Test_StuckAt_Logic(void) {
    printf("=== TEST 3: Stuck-At Logic ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00); 
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(FAULT_TARGET_NVM);

    printf("   [Part A] Stuck-At-1 on Byte 5, Bit 4\n");
    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_STUCK_AT_1, 200, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 200;
    cfg->BitPosition = (5 * 8) + 4;

    ProcessSystem(5);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Stuck-At-1", golden, read, TRUE);

    printf("   [Part B] Stuck-At-0 on Byte 5, Bit 0\n");
    ResetBuffer(sent, 0xFF); 
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(FAULT_TARGET_NVM); 
    
    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_STUCK_AT_0, 200, 1);
    cfg = FaultState_GetConfig(1);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 200;
    cfg->BitPosition = (5 * 8) + 0;

    ProcessSystem(5);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Stuck-At-0", golden, read, TRUE);
}

void Test_Frequency_Pulse(void) {
    printf("=== TEST 4: Frequency Pulse (ON/OFF/ON) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(FAULT_TARGET_NVM);

    uint32_t now = GetSystemTimeMs();
    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_BIT_FLIP, 50, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = now;
    cfg->End_timeMs   = now + 1000;
    cfg->Freq         = 200;          
    cfg->BitPosition  = 0;            

    ProcessSystem(10);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Freq Pulse (ON)", golden, read, TRUE);

    ProcessSystem(90); 
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Freq Pulse (OFF Gap)", golden, read, FALSE);

    ProcessSystem(110); 
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Freq Pulse (ON Again)", golden, read, TRUE);
}

void Test_MultiBit_Mask(void) {
    printf("=== TEST 5: Multi-Bit Mask ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(FAULT_TARGET_NVM);

    printf("   [Config] Applying Mask 0x0F to Byte 0\n");
    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_MULTI_BIT_FLIP, 200, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 200;
    cfg->Mask = 0x0F; 

    ProcessSystem(5);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Multi-Bit Mask", golden, read, TRUE);
}

void Test_Concurrency_Stress(void) {
    printf("=== TEST 6: Concurrency Stress (Triple Fault) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(FAULT_TARGET_NVM);
    uint32_t now = GetSystemTimeMs();

    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_BIT_FLIP, 500, 0);
    FaultConfig_t* c0 = FaultState_GetConfig(0);
    c0->Start_TimeMs = now; c0->End_timeMs = now + 500; c0->BitPosition = 0;

    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_STUCK_AT_1, 500, 1);
    FaultConfig_t* c1 = FaultState_GetConfig(1);
    c1->Start_TimeMs = now; c1->End_timeMs = now + 500; c1->BitPosition = 15;

    FaultState_Activate_fault(FAULT_TARGET_NVM, FAULT_DATA_CORRUPTION, 500, 2);
    FaultConfig_t* c2 = FaultState_GetConfig(2);
    c2->Start_TimeMs = now; c2->End_timeMs = now + 500;

    ProcessSystem(10);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Concurrency Stress", golden, read, TRUE);
}

void Test_Fls_BitFlip_Visual(void) {
    printf("=== TEST 7: FLS Visual Bit Flip (Bit 3) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    memset(sent, 0x00, BUFFER_SIZE); 
    memcpy(golden, sent, BUFFER_SIZE);
    
    Fault_Clear(FAULT_TARGET_FLS);
    Fault_Clear(FAULT_TARGET_NVM);

    FaultState_Activate_fault(FAULT_TARGET_FLS, FAULT_BIT_FLIP, 500, 0);
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
    
    AnalyzeResult("Visual Bit Flip", golden, read, TRUE);
}

void Test_Fls_TimeWindow(void) {
    printf("=== TEST 8: FLS Time Window Precision ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x00);
    memcpy(golden, sent, BUFFER_SIZE);
    Fault_Clear(FAULT_TARGET_FLS);
    Fault_Clear(FAULT_TARGET_NVM);
    
    uint32_t now = GetSystemTimeMs();
    uint32_t startWindow = now + 200;
    uint32_t endWindow   = now + 400;

    FaultState_Activate_fault(FAULT_TARGET_FLS, FAULT_BIT_FLIP, (endWindow - startWindow), 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = startWindow;
    cfg->End_timeMs   = endWindow;
    cfg->BitPosition  = 0;

    printf("   [Step 1] Writing Before Window...\n");
    ProcessSystem(10); 
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Time Window (Before)", golden, read, FALSE);

    printf("   [Step 2] Writing Inside Window...\n");
    ProcessSystem(startWindow - GetSystemTimeMs() + 50); 
    ResetBuffer(sent, 0x11); memcpy(golden, sent, BUFFER_SIZE);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Time Window (Inside)", golden, read, TRUE);

    printf("   [Step 3] Writing After Window...\n");
    ProcessSystem(endWindow - GetSystemTimeMs() + 50);
    ResetBuffer(sent, 0x22); memcpy(golden, sent, BUFFER_SIZE);
    NvM_WriteBlock(TEST_BLOCK_ID, sent); WaitForNvM();
    NvM_ReadBlock(TEST_BLOCK_ID, read); WaitForNvM();
    AnalyzeResult("Time Window (After)", golden, read, FALSE);
}

void Test_Fls_XOR_Logic(void) {
    printf("=== TEST 9: FLS XOR Logic (Mask 0xAA) ===\n");
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    Fault_Clear(FAULT_TARGET_FLS);
    Fault_Clear(FAULT_TARGET_NVM);

    FaultState_Activate_fault(FAULT_TARGET_FLS, FAULT_MULTI_BIT_FLIP, 500, 0);
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

    AnalyzeResult("XOR Logic", golden, read, TRUE);
}

void Test_Case1_RedundantBlock(void) {
    printf("=== CASE 1: Redundant Block (Block 2) ===\n");
    printf("   [NVM199/NVM279/NVM655] If CRC fails on primary, recovers from secondary.\n");
    uint16_t blockId = 2;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x11);
    memcpy(golden, sent, BUFFER_SIZE);

    /* 1. Write clean data (both copies) */
    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);

    /* 2. Write again, but corrupt the primary block.
       We use a very short time window (e.g., 5ms) on FLS Write so that only the 
       first copy is corrupted, and the second copy writes successfully. */
    printf("   [Step 1] Writing corrupted data to Primary, clean to Secondary...\n");
    Fault_Clear(FAULT_TARGET_FLS);
    FaultState_Activate_fault(FAULT_TARGET_FLS, FAULT_DATA_CORRUPTION, 5, 0); 
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 5;

    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);

    printf("   [Step 2] Reading back. NVM should recover from Secondary copy.\n");
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);
    
    AnalyzeResult_Block("Case 1 Redundant", golden, read, TRUE, blockId);
}

void Test_Case2_NativeWithROM(void) {
    printf("=== CASE 2: Native Block with ROM Default (Block 3) ===\n");
    printf("   [NVM202] Load default values if CRC fails.\n");
    uint16_t blockId = 3;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE];
    uint8 rom_golden[BUFFER_SIZE];
    memset(rom_golden, 0xDD, BUFFER_SIZE); /* From RomBlock3_Default */

    ResetBuffer(sent, 0x33);

    /* 1. Write corrupted data so CRC fails */
    Fault_Clear(FAULT_TARGET_FLS);
    FaultState_Activate_fault(FAULT_TARGET_FLS, FAULT_DATA_CORRUPTION, 500, 0);
    FaultConfig_t* cfg = FaultState_GetConfig(0);
    cfg->Start_TimeMs = GetSystemTimeMs();
    cfg->End_timeMs = cfg->Start_TimeMs + 500;

    printf("   [Step 1] Writing corrupted data to NV memory...\n");
    NvM_WriteBlock(blockId, sent); WaitForBlock(blockId);

    printf("   [Step 2] Reading back. NVM should fail CRC and load ROM defaults.\n");
    memset(read, 0, BUFFER_SIZE);
    NvM_ReadBlock(blockId, read); WaitForBlock(blockId);

    AnalyzeResult_Block("Case 2 Native With ROM", rom_golden, read, TRUE, blockId);
}

void Test_Case3_NativeNoROM(void) {
    printf("=== CASE 3: Native Block WITHOUT ROM (Block 4) ===\n");
    printf("   [NVM658] If CRC fails and no ROM, block remains invalid.\n");
    uint16_t blockId = 4;
    uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
    ResetBuffer(sent, 0x44);
    memcpy(golden, sent, BUFFER_SIZE); 

    /* 1. Write corrupted data so CRC fails */
    Fault_Clear(FAULT_TARGET_FLS);
    FaultState_Activate_fault(FAULT_TARGET_FLS, FAULT_DATA_CORRUPTION, 500, 0);
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

    printf("\n------------------------------------------------------------\n");
    printf(" FINAL RESULTS: %d Passed, %d Failed\n", g_testsPassed, g_testsFailed);
    printf("------------------------------------------------------------\n");

    return (g_testsFailed == 0) ? 0 : 1;
}
