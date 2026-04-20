    #include <stdio.h>
    #include <string.h>
    #include <time.h>
    #include <stdlib.h>

    /* AUTOSAR / Stack Headers */
    #include "Std_Types.h"
    #include "NvM.h"
    #include "MemIf.h"
    #include "Crc.h"

    /* Force fflush on all prints for debugging */
    #undef printf
    #define printf(...) do { fprintf(stdout, __VA_ARGS__); fflush(stdout); } while(0)

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

    /* =========================================================================
    * VISUALIZATION & ANALYSIS HELPERS
    * ========================================================================= */

    /*void PrintBuffer(const char* label, uint8* buf) {
        printf("%s:\n", label);
        for(int i = 0; i < 16; i++) {   // print first 16 bytes only
            printf("0x%02X ", buf[i]);
        }
        printf("\n");
    }*/


    void ResetBuffer(uint8* buf, uint8 startVal) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            buf[i] = (uint8)(startVal + i);
        }
    }

    void PrintBuffer(const char* label, uint8* buf, uint8* reference) {
        printf("%s:\n", label);
        for(int i = 0; i < 16; i++) {   // print first 16 bytes only
            if (reference != NULL && buf[i] != reference[i]) {
                printf("[0x%02X] ", buf[i]);  // Corrupted byte in brackets
            } else {
                printf("0x%02X ", buf[i]);     // Normal byte
            }
        }
        printf("\n");
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

    /* =========================================================================
    * TEST CASES
    * ========================================================================= */

    void Test_BitFlip_Immediate(void) {
        printf("=== TEST 1: Bit Flip Immediate (NVM Target) ===\n");
        printf("   Goal: Flip Bit 0 of Byte 10. Check if NvM CRC catches it.\n\n");

        uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];

        ResetBuffer(sent, 0x00);
        memcpy(golden, sent, BUFFER_SIZE);

        printf("[GOLDEN BUFFER - Before Write]\n");
        PrintBuffer("Golden", golden, NULL);  // No comparison needed

        Fault_Clear(TARGET_NVM_WRITE_BLOCK);

        FaultState_Activate_fault(TARGET_NVM_WRITE_BLOCK, FAULT_BIT_FLIP, 500, 0);
        FaultConfig_t* cfg = FaultState_GetConfig(0);
        cfg->Start_TimeMs = GetSystemTimeMs();
        cfg->End_timeMs   = cfg->Start_TimeMs + 500;
        cfg->BitPosition  = (10 * 8) + 0;

        printf("\n[FAULT INJECTION ACTIVE]\n");
        printf("Target: NVM \nType: BIT_FLIP \nByte 10, Bit 0 \n");
        printf("Injection Window: %u ms to %u ms\n\n", cfg->Start_TimeMs, cfg->End_timeMs);

        ProcessSystem(5);

        printf("[WRITE PHASE]\n");
        NvM_WriteBlock(TEST_BLOCK_ID, sent);
        WaitForNvM();
        
        printf(">>> FAULT INJECTED DURING WRITE <<<\n");
        PrintBuffer("Data Sent by NVM", sent, golden);  // Compare with golden - corrupted byte in []

        NvM_RequestResultType writeStatus;
        NvM_GetErrorStatus(TEST_BLOCK_ID, &writeStatus);
        printf("\n[NvM Status After Write]: ");
        switch(writeStatus) {
            case NVM_REQ_OK: 
                printf("NVM_REQ_OK (No error detected)\n"); 
                break;
            case NVM_REQ_INTEGRITY_FAILED: 
                printf("NVM_REQ_INTEGRITY_FAILED (CRC Error - CAUGHT!)\n"); 
                break;
            case NVM_REQ_NOT_OK: 
                printf("NVM_REQ_NOT_OK (Write failed)\n"); 
                break;
            case NVM_REQ_NV_INVALIDATED: 
                printf("NVM_REQ_NV_INVALIDATED (Header error)\n"); 
                break;
            default: 
                printf("Status Code: %d\n", writeStatus);
        }

        memset(read, 0, BUFFER_SIZE);

        printf("\n[READ PHASE]\n");
        NvM_ReadBlock(TEST_BLOCK_ID, read);
        WaitForNvM();

        PrintBuffer("Read Back", read, golden);  // Compare with golden - shows corrupted bytes in []
        
        NvM_RequestResultType readStatus;
        NvM_GetErrorStatus(TEST_BLOCK_ID, &readStatus);
        printf("\n[NvM Status After Read]: ");
        switch(readStatus) {
            case NVM_REQ_OK: 
                printf("NVM_REQ_OK\n"); 
                break;
            case NVM_REQ_INTEGRITY_FAILED: 
                printf("NVM_REQ_INTEGRITY_FAILED (CRC Error - CAUGHT!)\n"); 
                break;
            default: 
                printf("Status Code: %d\n", readStatus);
        }

        AnalyzeResult("BitFlip Immediate", golden, read, TRUE);
    }








    void Test_Fls_BitFlip_Visual(void) {
        printf("=== TEST 2: FLS Visual Bit Flip (Bit 3) ===\n");
        printf("   Goal: Flip Bit 3 of Byte 0 at FLS layer. Check if stack catches it.\n\n");

        uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
        
        memset(sent, 0x00, BUFFER_SIZE);
        memcpy(golden, sent, BUFFER_SIZE);

        printf("[GOLDEN BUFFER - Before Write]\n");
        PrintBuffer("Golden", golden, NULL);

        Fault_Clear(TARGET_FLS_WRITE);
        Fault_Clear(TARGET_NVM_WRITE_BLOCK);

        FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_BIT_FLIP, 500, 0);
        FaultConfig_t* cfg = FaultState_GetConfig(0);
        cfg->Start_TimeMs = GetSystemTimeMs();
        cfg->End_timeMs   = cfg->Start_TimeMs + 500;
        cfg->BitPosition  = 3;

        printf("\n[FAULT INJECTION ACTIVE]\n");
        printf("Target: FLS \nType: BIT_FLIP \nByte 0, Bit 3 \n");
        printf("Injection Window: %u ms to %u ms\n\n", cfg->Start_TimeMs, cfg->End_timeMs);

        ProcessSystem(10);

        printf("[WRITE PHASE]\n");
        NvM_WriteBlock(TEST_BLOCK_ID, sent);
        WaitForNvM();

        printf(">>> FAULT INJECTED DURING WRITE (FLS Layer) <<<\n");
        PrintBuffer("Data Sent by NVM", sent, golden);

        // Get NvM status after write
        NvM_RequestResultType writeStatus;
        NvM_GetErrorStatus(TEST_BLOCK_ID, &writeStatus);
        printf("\n[NvM Status After Write]: ");
        switch(writeStatus) {
            case NVM_REQ_OK: 
                printf("NVM_REQ_OK (No error detected)\n"); 
                break;
            case NVM_REQ_INTEGRITY_FAILED: 
                printf("NVM_REQ_INTEGRITY_FAILED (CRC Error - CAUGHT!)\n"); 
                break;
            case NVM_REQ_NOT_OK: 
                printf("NVM_REQ_NOT_OK (Write failed)\n"); 
                break;
            case NVM_REQ_NV_INVALIDATED: 
                printf("NVM_REQ_NV_INVALIDATED (Header error)\n"); 
                break;
            default: 
                printf("Status Code: %d\n", writeStatus);
        }

        memset(read, 0x00, BUFFER_SIZE);

        printf("\n[READ PHASE]\n");
        NvM_ReadBlock(TEST_BLOCK_ID, read);
        WaitForNvM();

        PrintBuffer("Read Back", read, golden);

        // Get NvM status after read
        NvM_RequestResultType readStatus;
        NvM_GetErrorStatus(TEST_BLOCK_ID, &readStatus);
        printf("\n[NvM Status After Read]: ");
        switch(readStatus) {
            case NVM_REQ_OK: 
                printf("NVM_REQ_OK\n"); 
                break;
            case NVM_REQ_INTEGRITY_FAILED: 
                printf("NVM_REQ_INTEGRITY_FAILED (CRC Error - CAUGHT!)\n"); 
                break;
            default: 
                printf("Status Code: %d\n", readStatus);
        }

    

        AnalyzeResult("Visual Bit Flip", golden, read, TRUE);
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
        /* Should write perfectly with clean checksum */

        /* Now, activate fault on READ path */
        FaultState_Activate_fault(TARGET_FLS_READ, FAULT_BIT_FLIP, 500, 0);
        FaultConfig_t* cfg = FaultState_GetConfig(0);
        cfg->Start_TimeMs = GetSystemTimeMs();
        cfg->End_timeMs   = cfg->Start_TimeMs + 500;
        cfg->BitPosition  = 0; /* Flip LSB of byte 0 */

        printf("\n[FAULT INJECTION ACTIVE ON READ]\n");
        printf("Target: FLS \nType: BIT_FLIP on READ \nByte 0, Bit 0 \n");
        ProcessSystem(10);

        memset(read, 0x00, BUFFER_SIZE);

        printf("\n[READ PHASE] - Reading back data (Fault should trigger here)...\n");
        NvM_ReadBlock(TEST_BLOCK_ID, read);
        WaitForNvM();

        PrintBuffer("Read Back from fault-injected FLS_Read", read, golden);

        AnalyzeResult("FLS Read Data Corruption", golden, read, TRUE);
    }

    void Test_Fls_Erase_DataCorruption(void) {
        printf("=== TEST 4: FLS Erase Data Corruption (Incomplete Erase) ===\n");
        printf("   Goal: Write data, then trigger block erase with FAULT active so it partially erases.\n\n");

        uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];
        
        memset(sent, 0xBB, BUFFER_SIZE);
        memcpy(golden, sent, BUFFER_SIZE);

        /* We need Fls_Erase, Fls_Read, Fls_Write prototypes */
        extern Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length);
        extern Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
        extern Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);

        /* 1. Write Data directly to raw Flash address 0 to bypass NVM */
        printf("[WRITE PHASE] - Writing pattern 0xBB directly to FLS...\n");
        Fault_Clear(TARGET_FLS_WRITE);
        Fault_Clear(TARGET_NVM_WRITE_BLOCK);
        
        Fls_Write(0, sent, BUFFER_SIZE);

        /* 2. Activate Fault and trigger erase */
        FaultState_Activate_fault(TARGET_FLS_ERASE, FAULT_BIT_FLIP, 500, 0); /* Any fault type triggers the incomplete erase */
        FaultConfig_t* cfg = FaultState_GetConfig(0);
        cfg->Start_TimeMs = GetSystemTimeMs();
        cfg->End_timeMs   = cfg->Start_TimeMs + 500;

        printf("\n[FAULT INJECTION ACTIVE ON ERASE]\n");
        printf("Target: FLS \nType: Incomplete Erase (Only first sector wiped) \n");
        ProcessSystem(10);

        printf("\n[ERASE PHASE] - Triggering Fls_Erase(0, %d) with fault active...\n", 4096);
        /* We ask to erase 4096 bytes (4 sectors). The fault should only let it erase 1 sector (1024 bytes).
        * If BUFFER_SIZE is 64, it's inside the FIRST sector, so it SHOULD be completely erased if 
        * TargetAddress is 0. Wait, if we want to see it survive, we should write at offset > 1024.
        */
        
        /* Actually let's write at offset 2048 (Sector 2) */
        uint32 offset = 2048;
        Fls_Write(offset, sent, BUFFER_SIZE);

        /* Trigger erase for the 4096 byte block (from 0 to 4096) */
        Fls_Erase(0, 4096);

        printf("\n[ERASE PHASE] - Triggered Fls_Erase(0, 4096) with fault active...\n");
        
        /* Process system so Fls_MainFunction runs and does the FLS022 verify pass */
        ProcessSystem(10);
        
        /* Let's clear fault so later tests run clean */
        Fault_Clear(TARGET_FLS_ERASE);

        /* Check what FLS thinks happened! */
        extern MemIf_JobResultType Fls_GetJobResult(void);
        MemIf_JobResultType result = Fls_GetJobResult();
        
        if (result == MEMIF_JOB_FAILED) {
            printf("   Detector: FLS Module (via Fls_MainFunction FLS022 verify)\n");
            printf("   Status: Fault Injected Successfully (Stack CAUGHT the incomplete erase!).\n");
            printf("   RESULT:   [PASS]\n");
            extern int g_testsPassed; g_testsPassed++;
        } else {
            printf("   Detector: Application\n");
            printf("   Status: Fault Failed (Stack MISSED IT, result=%d).\n", result);
            printf("   RESULT:   [FAIL]\n");
            extern int g_testsFailed; g_testsFailed++;
        }
        printf("============================================================\n\n");
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
            
        
        /* Test_BitFlip_Immediate(); */
        /* Test_Fls_BitFlip_Visual(); */
        Test_Fls_Read_DataCorruption();
        Test_Fls_Erase_DataCorruption();
        //Test_Fls_Erase_SectorOne_SilentFailure();


        printf("\n------------------------------------------------------------\n");
        printf(" FINAL RESULTS: %d Passed, %d Failed\n", g_testsPassed, g_testsFailed);
        printf("------------------------------------------------------------\n");

        return (g_testsFailed == 0) ? 0 : 1;
    }
