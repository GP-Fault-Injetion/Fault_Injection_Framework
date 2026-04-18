#ifndef OS_H
#define OS_H

#include "Std_Types.h"

// 1. Define the status type and error code
typedef uint8 StatusType;
#define E_OS_ASSERT     1  

// 2. Define empty interrupt macros for PC simulation
#define SuspendAllInterrupts()
#define ResumeAllInterrupts()
#define SYS_CALL_SuspendOSInterrupts()
#define SYS_CALL_ResumeOSInterrupts()
#define DisableAllInterrupts()
#define EnableAllInterrupts()

// 3. Add the prototype for ShutdownOS
void ShutdownOS(StatusType Error);

#endif /* OS_H */