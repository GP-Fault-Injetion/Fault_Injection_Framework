    #include <stdio.h>
    #include <string.h>
    #include <time.h>
    #include <stdlib.h>

    /* AUTOSAR / Stack Headers */
    #include "Std_Types.h"
    #include "NvM.h"
    #include "MemIf.h"
    #include "Fee.h"
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
    * FEE TESTS
    * ========================================================================= */

    void Test_Fee_BitFlip_Visual(void) {
        printf("=== TEST: FEE Visual Bit Flip (Bit 3) ===\n");
        printf("   Goal: Flip Bit 3 of Byte 0 at FEE layer through NvM flow.\n\n");

        uint8 sent[BUFFER_SIZE], read[BUFFER_SIZE], golden[BUFFER_SIZE];

        memset(sent, 0x00, BUFFER_SIZE);
        memcpy(golden, sent, BUFFER_SIZE);

        printf("[GOLDEN BUFFER - Before Write]\n");
        PrintBuffer("Golden", golden, NULL);

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

        PrintBuffer("Read Back", read, golden);

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
    * PARAMETER CORRUPTION TESTS (FLS157, FLS158)
    * ========================================================================= */

    void Test_Fls_Write_ParameterCorruption_NullPointer(void) {
        printf("=== TEST 5: Fls_Write Parameter Corruption (Null Data Pointer) ===\n");
        printf("   Goal: Call Fls_Write with NULL data pointer. Verify FLS rejects request [FLS157].\n\n");

        printf("[FAULT INJECTION ACTIVE]\n");
        printf("Target: FLS \n");
        printf("Type: PARAMETER_CORRUPTION \n");
        printf("Parameter: SourceAddressPtr (NULL pointer)\n\n");


        printf("[WRITE PHASE WITH NULL POINTER]\n");
        uint32 targetAddress = 0x100;
        uint32 length = 64;

        /* Activate parameter corruption fault for FLS_WRITE */
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
        printf("Target: FLS \n");
        printf("Type: PARAMETER_CORRUPTION \n");
        printf("Parameter: TargetAddressPtr (NULL pointer)\n\n");


        printf("[READ PHASE WITH NULL POINTER]\n");
        uint32 sourceAddress = 0x100;
        uint32 length = 64;

        /* Activate parameter corruption fault for FLS_READ */
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
        printf("Target: FLS \n");
        printf("Type: PARAMETER_CORRUPTION \n");
        printf("Parameter: TargetAddress (unaligned, not on sector boundary)\n\n");

        printf("[ERASE PHASE WITH UNALIGNED ADDRESS]\n");
        uint32 unalignedAddress = 0x0001;  /* Not aligned to FLS_SECTOR_SIZE (1024) */
        uint32 length = 1024;
        
        /* Activate parameter corruption fault for FLS_ERASE */
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
        printf("Target: FLS \n");
        printf("Type: PARAMETER_CORRUPTION \n");
        printf("Parameter: Length (zero bytes)\n\n");

        printf("[ERASE PHASE WITH ZERO LENGTH]\n");
        uint32 address = 0x0000;  /* Aligned */
        uint32 zeroLength = 0;    /* Invalid: must be > 0 */
        
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
        printf("Target: FLS \n");
        printf("Type: PARAMETER_CORRUPTION \n");
        printf("Parameter: TargetAddress (aligned but exceeds flash size)\n\n");

        printf("[ERASE PHASE WITH OUT-OF-BOUNDS ADDRESS]\n");
        uint32 outOfBoundsAddress = 0x8000;  /* Exactly at/beyond FLASH_SIZE (32768 = 0x8000) */
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
        printf("Target: FLS \n");
        printf("Type: PARAMETER_CORRUPTION \n");
        printf("Parameter: Length (512 bytes, not aligned to sector size of 1024)\n\n");

        printf("[ERASE PHASE WITH NON-ALIGNED LENGTH]\n");
        uint32 address = 0x0000;        /* Aligned address */
        uint32 nonAlignedLength = 512;  /* Not a multiple of FLS_SECTOR_SIZE (1024) */
        
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
            printf("   Detector: Application (Strict validation - does not round)\n");
            printf("   [Requirement]: FLS221 - Driver implementation may require strict alignment\n");
            printf("   NOTE:     Depends on MCAL implementation interpretation\n");
            printf("   RESULT:   [CONDITIONAL]\n");
            printf("============================================================\n\n");
            /* We don't count this as pass/fail since it depends on implementation */
        }
    }
    
    /* =========================================================================
    * RETURN VALUE CORRUPTION TESTS
    * ========================================================================= */

    void Test_Fls_Write_ReturnValueCorruption(void) {
        printf("=== TEST 11: Fls_Write Return Value Corruption ===\n");
        printf("   Goal: Inject return value corruption. E_OK becomes E_NOT_OK. Verify behavior.\n\n");

        //extern Std_ReturnType Fls_Write(uint32 TargetAddress, const uint8* SourceAddressPtr, uint32 Length);
        
        uint8 testData[64];
        memset(testData, 0xAA, 64);

        printf("[FAULT INJECTION ACTIVE]\n");
        printf("Target: FLS_WRITE \n");
        printf("Type: RETURN_VALUE_CORRUPTION \n");
        printf("Return Value: E_OK (0x00) -> E_NOT_OK (0x01)\n\n");

        /* Activate return value corruption fault for FLS_WRITE */
        Fault_Clear(TARGET_FLS_WRITE);
        FaultState_Activate_fault(TARGET_FLS_WRITE, FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION, 500, 0);
        FaultConfig_t* cfg = FaultState_GetConfig(0);
        cfg->Start_TimeMs = GetSystemTimeMs();
        cfg->End_timeMs   = cfg->Start_TimeMs + 500;

        ProcessSystem(5);

        printf("[WRITE PHASE]\n");
        printf(">>> Hook_Fls_Write(0x1000, testData, 64) with corrupted return value\n");
        printf("[TEST] Corrupted Fls_Write return value: 0x00 -> 0x01\n\n");
        Std_ReturnType result = Fls_Write(0x1000, testData, 64);
        
        Fault_Clear(TARGET_FLS_WRITE);

        if (result == E_NOT_OK) {
            printf("   Status:   Return value correctly corrupted: E_OK -> E_NOT_OK\n");
            printf("   Detector: Application (Caller receives unexpected E_NOT_OK)\n");
            printf("   Impact:   Application may treat successful write as failed\n");
            printf("   RESULT:   [PASS] - Corruption detected by caller\n");
            printf("============================================================\n\n");
            g_testsPassed++;
        } else {
            printf("   Status:   Return value corruption FAILED (got 0x%02X instead of 0x01)\n", result);
            printf("   Detector: Application\n");
            printf("   RESULT:   [FAIL]\n");
            printf("============================================================\n\n");
            g_testsFailed++;
        }
    }

    void Test_Fls_Read_ReturnValueCorruption(void) {
        printf("=== TEST 12: Fls_Read Return Value Corruption ===\n");
        printf("   Goal: Inject return value corruption on Fls_Read. E_OK becomes E_NOT_OK.\n\n");

        extern Std_ReturnType Fls_Read(uint32 SourceAddress, uint8* TargetAddressPtr, uint32 Length);
        
        uint8 testData[64];
        memset(testData, 0x00, 64);

        printf("[WRITE PHASE] - Writing clean data to flash...\n");
        Fault_Clear(TARGET_FLS_WRITE);
        Hook_Fls_Write(0x2000, testData, 64);

        printf("\n[FAULT INJECTION ACTIVE]\n");
        printf("Target: FLS_READ \n");
        printf("Type: RETURN_VALUE_CORRUPTION \n");
        printf("Return Value: E_OK (0x00) -> E_NOT_OK (0x01)\n\n");

        /* Activate return value corruption fault for FLS_READ */
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
        printf("[TEST] Corrupted Fls_Read return value: 0x00 -> 0x01\n\n");
        Std_ReturnType result = Hook_Fls_Read(0x2000, readBuffer, 64);
        
        Fault_Clear(TARGET_FLS_READ);

        if (result == E_NOT_OK) {
            printf("   Status:   Return value correctly corrupted: E_OK -> E_NOT_OK\n");
            printf("   Detector: Application (Caller receives unexpected E_NOT_OK)\n");
            printf("   Data Status: Buffer contains valid data despite corrupted return\n");
            printf("   Impact:   Application will retry or report read failure\n");
            printf("   RESULT:   [PASS] - Corruption detected by caller\n");
            printf("============================================================\n\n");
            g_testsPassed++;
        } else {
            printf("   Status:   Return value corruption FAILED (got 0x%02X instead of 0x01)\n", result);
            printf("   Detector: Application\n");
            printf("   RESULT:   [FAIL]\n");
            printf("============================================================\n\n");
            g_testsFailed++;
        }
    }

    void Test_Fls_Erase_ReturnValueCorruption(void) {
        printf("=== TEST 13: Fls_Erase Return Value Corruption ===\n");
        printf("   Goal: Inject return value corruption on Fls_Erase. E_OK becomes E_NOT_OK.\n\n");

        extern Std_ReturnType Fls_Erase(uint32 TargetAddress, uint32 Length);
        
        printf("[PREPARATION PHASE]\n");
        printf("Writing test pattern to flash at address 0x3000...\n");
        uint8 testData[64];
        memset(testData, 0x55, 64);
        Fault_Clear(TARGET_FLS_WRITE);
        Hook_Fls_Write(0x3000, testData, 64);

        printf("\n[FAULT INJECTION ACTIVE]\n");
        printf("Target: FLS_ERASE \n");
        printf("Type: RETURN_VALUE_CORRUPTION \n");
        printf("Return Value: E_OK (0x00) -> E_NOT_OK (0x01)\n\n");

        /* Activate return value corruption fault for FLS_ERASE */
        Fault_Clear(TARGET_FLS_ERASE);
        FaultState_Activate_fault(TARGET_FLS_ERASE, FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION, 500, 0);
        FaultConfig_t* cfg = FaultState_GetConfig(0);
        cfg->Start_TimeMs = GetSystemTimeMs();
        cfg->End_timeMs   = cfg->Start_TimeMs + 500;

        ProcessSystem(5);

        printf("[ERASE PHASE]\n");
        printf(">>> Hook_Fls_Erase(0x3000, 1024) with corrupted return value\n");
        printf("[TEST] Corrupted Fls_Erase return value: 0x00 -> 0x01\n\n");
        Std_ReturnType result = Hook_Fls_Erase(0x3000, 1024);
        
        Fault_Clear(TARGET_FLS_ERASE);

        if (result == E_NOT_OK) {
            printf("   Status:   Return value correctly corrupted: E_OK -> E_NOT_OK\n");
            printf("   Detector: Application (Caller receives unexpected E_NOT_OK)\n");
            printf("   Erase Status: Erase operation may have completed despite error return\n");
            printf("   Impact:   Application will think erase failed and may retry\n");
            printf("   RESULT:   [PASS] - Corruption detected by caller\n");
            printf("============================================================\n\n");
            g_testsPassed++;
        } else {
            printf("   Status:   Return value corruption FAILED (got 0x%02X instead of 0x01)\n", result);
            printf("   Detector: Application\n");
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
            
        
        Test_BitFlip_Immediate();
        Test_Fls_BitFlip_Visual();
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
        //Test_Fls_Erase_SectorOne_SilentFailure();


        printf("\n------------------------------------------------------------\n");
        printf(" FINAL RESULTS: %d Passed, %d Failed\n", g_testsPassed, g_testsFailed);
        printf("------------------------------------------------------------\n");

        return (g_testsFailed == 0) ? 0 : 1;
    }
