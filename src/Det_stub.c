#include "Det.h"
#include <stdio.h>

void Det_ReportError(uint16 ModuleId,
                     uint8 InstanceId,
                     uint8 ApiId,
                     uint8 ErrorId)
{
    (void)ModuleId; (void)InstanceId; (void)ApiId; (void)ErrorId;
    /* Minimal handler for test builds */
}
