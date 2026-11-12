#ifndef HOST_FREERTOS_H
#define HOST_FREERTOS_H

/* Minimal single-threaded host substitutes. The interpreter and callbacks are
 * the production sources, not mocks. No scheduler/interrupt behavior is modeled. */
#include <stddef.h>
#include <stdint.h>
#include "FreeRTOSConfig.h"

typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;
#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)
#define pdFAIL pdFALSE
#define pdPASS pdTRUE

void *pvPortMalloc(size_t size);

#endif
