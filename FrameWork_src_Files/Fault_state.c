#include "Fault_state.h"
#include <time.h> /* Added for Real Time */

/* Global array to hold faults */
static FaultConfig_t Fault_Table[MAX_ACTIVE_FAULTS];

void FaultState_Init(void) {
    uint16_t i;
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        Fault_Table[i].Active = FALSE;
        Fault_Table[i].running = FALSE;
        Fault_Table[i].TargetModuleServiceID = 0;
        Fault_Table[i].Type = FAULT_NONE;
        Fault_Table[i].current_start_time = 0;
    }
}

/* * Implementation using standard time.h 
 * Returns milliseconds since the program started.
 */
uint32_t Fault_GetTimeMs(void) {
    clock_t ticks = clock();
    /* Convert clock ticks to milliseconds */
    return (uint32_t)((ticks * 1000) / CLOCKS_PER_SEC);
}

/* Internal update logic for a single fault */
static void FaultState_Update_Single(FaultConfig_t* fault) {
    uint32_t currentTime = Fault_GetTimeMs();

    /* 1. Global Time Window Check (Is the fault physically "Running"?) */
    if ((currentTime >= fault->Start_TimeMs) && (currentTime < fault->End_timeMs)) {
        fault->running = TRUE;
    } else {
        fault->running = FALSE;
        fault->Active = FALSE;
        return; 
    }

    /* 2. Frequency Logic (Periodic Activation) */
    if (fault->current_start_time == 0) {
        /* First run initialization */
        fault->current_start_time = fault->Start_TimeMs;
    }

    /* Catch-up loop: Ensure current_start_time is the start of the *current* cycle */
    if (fault->Freq > 0) {
        while (currentTime >= (fault->current_start_time + fault->Freq)) {
            fault->current_start_time += fault->Freq;
        }
    }

    /* 3. Duration Window Check (Is the fault "Active" right now?) */
    uint32_t cycleEndTime = fault->current_start_time + fault->DurationMs;

    if (currentTime >= fault->current_start_time && currentTime < cycleEndTime) {
        fault->Active = TRUE;
    } else {
        fault->Active = FALSE;
    }
}

/* * NEW: Main Function to be called cyclically by the OS/Main Loop.
 * This updates the state of all faults regardless of Hook calls.
 */
void FaultState_MainFunction(void) {
    uint16_t i;
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        /* Only process faults that are configured (Type != NONE) to save time */
        if (Fault_Table[i].Type != FAULT_NONE) {
            FaultState_Update_Single(&Fault_Table[i]);
        }
    }
}

/* API to activate a fault configuration */
Std_ReturnType FaultState_Activate_fault(uint16_t moduleId, FaultType_t type, uint32_t duration, uint16_t fault_Id) {
    if (fault_Id >= MAX_ACTIVE_FAULTS) {
        return E_NOT_OK;
    }

    FaultConfig_t* fault = &Fault_Table[fault_Id];
    
    fault->TargetModuleServiceID = moduleId;
    fault->Type = type;
    fault->DurationMs = duration;
    
    /* Reset Runtime States */
    fault->Active = FALSE;
    fault->running = FALSE;
    fault->current_start_time = 0; 
    
    return E_OK;
}

boolean FaultState_IsActive(uint16_t module_service_Id, uint16_t fault_Id) {
    if (fault_Id >= MAX_ACTIVE_FAULTS) {
        return FALSE;
    }
    
    if ((Fault_Table[fault_Id].TargetModuleServiceID == module_service_Id) &&
        (Fault_Table[fault_Id].Active == TRUE)) {
        return TRUE;
    }
    
    return FALSE;
}

void FaultState_Clear(uint16_t moduleId) {
    uint16_t i;
    for (i = 0; i < MAX_ACTIVE_FAULTS; i++) {
        if (Fault_Table[i].TargetModuleServiceID == moduleId) {
            Fault_Table[i].Type = FAULT_NONE;
            Fault_Table[i].Active = FALSE;
            Fault_Table[i].running = FALSE;
        }
    }
}

/* Helper for Hook to get config */
FaultConfig_t* FaultState_GetConfig(uint16_t fault_Id) {
    if (fault_Id < MAX_ACTIVE_FAULTS) {
        return &Fault_Table[fault_Id];
    }
    return NULL;
}
