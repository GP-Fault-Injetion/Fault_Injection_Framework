#include "Det.h"
#include <stdio.h>

uint16 Last_Det_Error = 0;
void Det_ReportError(uint16 ModuleId,
                     uint8 InstanceId,
                     uint8 ApiId,
                     uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId; (void)ApiId; 
    /* Capture the specific AUTOSAR Error ID for our test assertions */
    Last_Det_Error = ErrorId;
    /* Minimal handler for test builds */
}
