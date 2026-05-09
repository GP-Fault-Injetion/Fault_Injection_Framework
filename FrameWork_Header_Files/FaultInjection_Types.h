#ifndef FAULTINJECTION_TYPES_H
#define FAULTINJECTION_TYPES_H

#include "Std_Types.h"
#include "Target_Stack.h"

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


/**
 * @brief Enumeration of supported fault types.
 */
typedef enum {
    FAULT_NONE = 0,

    /* DATA family */
    FAULT_BIT_FLIP,
    FAULT_MULTI_BIT_FLIP,
    FAULT_STUCK_AT_0,
    FAULT_STUCK_AT_1,
    FAULT_DATA_CORRUPTION,
    FAULT_CRC_DATA_CORRUPTION,

    /* TIMING family */
    FAULT_DELAY,

    /* CONTROL_FLOW family */
    FAULT_OMISSION,

    /* INTERFACE family */
    FAULT_RETURN_VALUE_REJECTION,                /* do not call real API */
    FAULT_RETURN_VALUE_OBSERVATION_CORRUPTION,   /* call real API, then change returned value */
    FAULT_PARAMETER_CORRUPTION,
    FAULT_QUEUE_OVERFLOW,

    /* STATE family */
    FAULT_MODULE_STATE_CORRUPTION,

    /* POWER family */
    FAULT_POWER_LOSS
} FaultType_t;

/**
 * @brief Trigger modes for fault injection.
 */


/**
 * @brief Configuration parameters for a single fault injection.
 */
typedef struct {
    FaultType_t Type;                   /**< Type of fault to inject */
    uint16_t TargetModuleServiceID;           /**< ID of the module to target */
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
