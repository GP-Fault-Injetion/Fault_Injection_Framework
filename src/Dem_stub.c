#include "Dem.h"
#include <stdio.h>
#include "Std_Types.h"

void Dem_ReportErrorStatus(uint16 EventId,
                           Dem_EventStatusType EventStatus)
{
    printf("DEM error: Event=%d Status=%d\n",
           EventId, EventStatus);
}
