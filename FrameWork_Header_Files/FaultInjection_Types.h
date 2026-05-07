#ifndef FAULTINJECTION_TYPES_H
#define FAULTINJECTION_TYPES_H

#include "Std_Types.h"

/**
 * @file FaultInjection_Types.h
 * @brief Global type definitions for the Fault Injection Framework (FR-01).
 */

/**
 * @name Target Module&Service IDs (Memory Stack)
 * @brief Standard AUTOSAR IDs used to identify the target for injection. Format: 0x[ModuleID_Hex][ServiceID_Hex]
 * @{
 */

/* --- NvM Targets --- */
#define TARGET_NVM_INIT                   0x1400
#define TARGET_NVM_SET_DATA_INDEX         0x1401
#define TARGET_NVM_GET_ERROR_STATUS       0x1404
#define TARGET_NVM_READ_BLOCK             0x1406
#define TARGET_NVM_WRITE_BLOCK            0x1407
#define TARGET_NVM_READ_ALL               0x140C
#define TARGET_NVM_WRITE_ALL              0x140D
#define TARGET_NVM_INVALIDATE_NV_BLOCK    0x140F
#define TARGET_NVM_ERASE_NV_BLOCK         0x1410
#define TARGET_NVM_MAIN_FUNCTION          0x1411

#define FAULT_TARGET_NVM                  0x1400

/* --- Fee Targets --- */
#define TARGET_FEE_INIT                   0x1500
#define TARGET_FEE_READ                   0x1502
#define TARGET_FEE_WRITE                  0x1503
#define TARGET_FEE_GET_STATUS             0x1505
#define TARGET_FEE_GET_JOB_RESULT         0x1506
#define TARGET_FEE_INVALIDATE_BLOCK       0x1507
#define TARGET_FEE_MAIN_FUNCTION          0x1512

/* --- MemIf Targets --- */
#define TARGET_MEMIF_READ                 0x1602
#define TARGET_MEMIF_WRITE                0x1603
#define TARGET_MEMIF_GET_STATUS           0x1605
#define TARGET_MEMIF_GET_JOB_RESULT       0x1606
#define TARGET_MEMIF_INVALIDATE_BLOCK     0x1607
#define TARGET_MEMIF_ERASE_IMM_BLOCK      0x1608

/* --- Fls Targets --- */
#define TARGET_FLS_INIT                   0x5C00
#define TARGET_FLS_ERASE                  0x5C01
#define TARGET_FLS_WRITE                  0x5C02
#define TARGET_FLS_READ                   0x5C03
#define TARGET_FLS_GET_STATUS             0x5C05
#define TARGET_FLS_GET_JOB_RESULT         0x5C06
#define TARGET_FLS_MAIN_FUNCTION          0x5CFF

#define FAULT_TARGET_FLS                  0x5C00
/** @} */

/**
 * @brief Enumeration of supported fault types.
 */
typedef enum {
    FAULT_NONE,
    FAULT_BIT_FLIP,
    FAULT_MULTI_BIT_FLIP,
    FAULT_STUCK_AT_0,
    FAULT_STUCK_AT_1,
    FAULT_DELAY,
    FAULT_OMISSION,
    FAULT_DATA_CORRUPTION,
    FAULT_CRC_DATA_CORRUPTION,
    FAULT_RETURN_VALUE_CORRUPTION,
    FAULT_PARAMETER_CORRUPTION,
    FAULT_QUEUE_OVERFLOW
} FaultType_t;

/**
 * @brief Trigger modes for fault injection.
 */


/**
 * @brief Configuration parameters for a single fault injection.
 */
typedef struct {
    FaultType_t Type;                   /**< Type of fault to inject */
    uint16_t TargetModuleServiceID;           /**< ID of the module and service  to target */
    uint16_t FaultID;                 /**< Unique identifier for the fault instance */

    
    boolean  Active;                   /**< Master enable/disable switch */
    uint32_t  running;                 //the error running state

    
    
      
    uint32_t Start_TimeMs;             /**< Value for the trigger (e.g., 100ms or 5th call) */
    uint32_t DurationMs;                   /**< How long the fault lasts */
    uint32_t End_timeMs;                   /**< Time to stop injecting the fault */
    uint32_t Freq;                         ///**< Frequency of fault injection */
    uint32_t current_start_time;          /**< Current start time tracking */
    
    uint8_t BitPosition;               /**< Specific bit to flip (if applicable) */
    uint32_t Mask;                     /**< Mask for MultiBit flip(if applicable)*/
} FaultConfig_t;

#endif /* FAULTINJECTION_TYPES_H */
