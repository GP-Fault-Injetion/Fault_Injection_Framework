#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>

/* AUTOSAR Platform types */
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef char     char_t;

typedef int8_t   sint8;
typedef int16_t  sint16;
typedef int32_t  sint32;

/* Boolean type */
typedef uint8 boolean;

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* CPU type (simplified) */
#define CPU_TYPE_8   8
#define CPU_TYPE_16  16
#define CPU_TYPE_32  32

#define CPU_TYPE CPU_TYPE_32

#endif /* PLATFORM_TYPES_H */
