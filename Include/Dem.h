#ifndef DEM_H
#define DEM_H

#include "Std_Types.h"

typedef uint8 Dem_EventStatusType;

#define DEM_EVENT_STATUS_PASSED  0
#define DEM_EVENT_STATUS_FAILED  1

void Dem_ReportErrorStatus(uint16 EventId,
                           Dem_EventStatusType EventStatus);

#endif
