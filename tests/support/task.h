#ifndef HOST_TASK_H
#define HOST_TASK_H

#include "FreeRTOS.h"
/* Host tests run sequentially, with no interrupts. */
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)

#endif
