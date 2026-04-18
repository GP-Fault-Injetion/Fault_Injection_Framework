/* include/MemIf_Cfg.h */
#ifndef MEMIF_CFG_H_
#define MEMIF_CFG_H_

#include "MemIf_Types.h"

#define MEMIF_VERSION_INFO_API          STD_ON
#define MEMIF_DEV_ERROR_DETECT          STD_ON
#define USE_FEE    STD_ON  /* or just #define USE_FEE */

/* We use 1 Device (Fee) */
#define MEMIF_NUMBER_OF_DEVICES         1

/* Device 0 is the Fee */
#define MEMIF_FEE_INDEX                 0

#endif /* MEMIF_CFG_H_ */