/**
 * @file FaultInjection_Interface.h
 * @brief Interface definitions for the Fault Injection module.
 */
#ifndef FAULTINJECTION_INTERFACE_H
#define FAULTINJECTION_INTERFACE_H
#include "FaultInjection_Types.h"
#include "FaultBitLogic.h"
#include "Fault_DataLogic.h"
#include "Fault_state.h"

/**
 * @brief Initializes the Fault Injection module
 * @return E_OK if successful.
 */
Std_ReturnType Fault_Init(void);

/**
 * @brief Injects a fault into the data buffer based on configuration.
 * @param[in,out] data  Pointer to the data buffer to modify.
 * @param[in]     length Size of the data buffer.
 * @param[in]     config Fault parameters (Type, Duration, BitPosition).
 * @return E_OK if fault was processed or ignored safely.
 */
Std_ReturnType Fault_Inject(uint8_t* data,uint32_t length,FaultConfig_t* config);

/**
 * @brief Resets active faults (like Stuck-At) for a specific module( Stops the fault)
 * @param[in] TargetModuleID  ID of the module to clear.
 */

void Fault_Clear(uint16_t FaultID); // Clears the fault configuration for a specific Fault

#endif /* FAULTINJECTION_INTERFACE_H */
