#ifndef FAULT_STATE_H
#define FAULT_STATE_H

#include "Std_Types.h"
#include "FaultInjection_Types.h"
/*----------------------------------------------------------
 * Fault State Entry (runtime only)
 *----------------------------------------------------------*/
#define MAX_ACTIVE_FAULTS   10



/*----------------------------------------------------------
 * API Functions 
 *----------------------------------------------------------*/

void FaultState_Init(void);

uint32_t Fault_GetTimeMs(void);

void FaultState_Update_fault(uint16_t fault_Id);

boolean FaultState_IsActive(uint16_t moduleId,uint16_t fault_Id);

Std_ReturnType FaultState_Activate_fault(uint16_t moduleId, FaultType_t type, uint32_t duration, uint16_t fault_Id);
void FaultState_Clear(uint16_t fault_Id); 

boolean Fault_ShouldInject();

void FaultState_MainFunction(void);

FaultConfig_t* FaultState_GetConfig(uint16_t fault_Id);








#endif