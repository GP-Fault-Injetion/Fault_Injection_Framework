#ifndef STD_TYPES_H
#define STD_TYPES_H

#include "Platform_Types.h"

/* AUTOSAR return type */
typedef uint8 Std_ReturnType;

/* Block ID type */
typedef uint16 NvM_BlockIdType;

/* Request result type */
typedef uint8 NvM_RequestResultType;


#define E_OK     0u
#define E_NOT_OK 1u

#ifndef NULL_PTR
  #define NULL_PTR  ((void *)0)
#endif

#endif /* STD_TYPES_H */
