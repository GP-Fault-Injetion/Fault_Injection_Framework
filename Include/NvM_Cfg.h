/*-------------------------------- Arctic Core ------------------------------
 * Copyright (C) 2013, ArcCore AB, Sweden, www.arccore.com.
 * Contact: <contact@arccore.com>
 * 
 * You may ONLY use this file:
 * 1)if you have a valid commercial ArcCore license and then in accordance with
 * the terms contained in the written license agreement between you and ArcCore,
 * or alternatively
 * 2)if you follow the terms found in GNU General Public License version 2 as
 * published by the Free Software Foundation and appearing in the file
 * LICENSE.GPL included in the packaging of this file or here
 * <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>
 *-------------------------------- Arctic Core -----------------------------*/


    
/* include/NvM_Cfg.h */
#ifndef NVM_CFG_H_
#define NVM_CFG_H_

#include "NvM_Types.h"
#include "NvM_ConfigTypes.h"

/* --- Feature Switches --- */
#define NVM_DEV_ERROR_DETECT            STD_ON
#define NVM_VERSION_INFO_API            STD_ON
#define NVM_SET_RAM_BLOCK_STATUS_API    STD_ON   /* Needed to mark block as 'changed' */
#define NVM_POLLING_MODE                STD_ON   /* CRITICAL: Must be ON for main loop */

/* --- Queue & Size Config --- */
#define NVM_SIZE_STANDARD_JOB_QUEUE     5        /* Size of the Job Queue */
#define NVM_SIZE_IMMEDIATE_JOB_QUEUE    5        /* Size of the High Priority Queue */
#define NVM_NUM_OF_NVRAM_BLOCKS         5        /* Block 0 + Block 1 */

/* --- Default Settings --- */
#define NVM_API_CONFIG_CLASS            3 
#define NVM_CRC_NUM_OF_BYTES            2        /* 2 Bytes for CRC16 */
#define NVM_DATASET_SELECTION_BITS      0
#define NVM_DRV_MODE_SWITCH             STD_OFF
#define NVM_DYNAMIC_CONFIGURATION       STD_OFF
#define NVM_JOB_PRIORIZATION            STD_OFF
#define NVM_MAX_NUMBER_OF_WRITE_RETRIES 1
#define NVM_MAX_NUMBER_OF_READ_RETRIES  5
#define NVM_MAX_BLOCK_LENGTH            512      /* Max block length in bytes */

#endif /* NVM_CFG_H_ */
