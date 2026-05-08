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


#warning "This default file may only be used as an example!"

#include "Fee.h"
#include "Fee_Cfg.h"
#include "Fee_ConfigTypes.h"

/* Callbacks needed by Fee_ConfigTypes.h */
void NvM_JobEndNotification(void);
void NvM_JobErrorNotification(void);

/* Defined in Fee_Cfg.h as 8*/
const Fee_BlockConfigType Fee_BlockConfig[FEE_NUM_OF_BLOCKS] = {
    /* Block 0: RESERVED/CONFIG Block (Mapped to NvM Block 1) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 1,
        .BlockSize = 66,
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    },
    /* Block 1: DATA Block (Mapped to NvM Block 2 - Redundant Primary) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 2,
        .BlockSize = 66,
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    },
    /* Block 2: DATA Block (Mapped to NvM Block 2 - Redundant Secondary) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 3,
        .BlockSize = 66,
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    },
    /* Block 3: DATA Block (Mapped to NvM Block 3 - Native with ROM) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 4,
        .BlockSize = 66,
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    },
    /* Block 4: DATA Block (Mapped to NvM Block 4 - Native no ROM) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 5,
        .BlockSize = 66,
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    },
    
    /* --- NEW BLOCKS ADDED FOR FREEDOM --- */
    
    /* Block 5: DATA Block (Mapped to Dataset Slot 0) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 6,
        .BlockSize = 66,   /* 64 bytes data + 2 bytes CRC */
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    },
    /* Block 6: DATA Block (Mapped to Dataset Slot 1) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 7,
        .BlockSize = 66,
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    },
    /* Block 7: Spare DATA Block (Buffer to hit 8 blocks) */
    {
        .DeviceIndex = 0,
        .BlockNumber = 8,
        .BlockSize = 66,
        .ImmediateData = FALSE,
        .NumberOfWriteCycles = 0
    }
};

/* Main Configuration Container */
const Fee_ConfigType Fee_Config = {
    .General = {
        .NvmJobEndCallbackNotificationCallback = NvM_JobEndNotification,
        .NvmJobErrorCallbackNotificationCallback = NvM_JobErrorNotification
    },
    .BlockConfig = Fee_BlockConfig
};
