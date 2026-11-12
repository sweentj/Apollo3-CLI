#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* Standalone console example: 48 MHz core, ordinary SysTick, no deep sleep. */
#define configUSE_PREEMPTION 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ 48000000UL
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 4
#define configMINIMAL_STACK_SIZE 256
#define configTOTAL_HEAP_SIZE (16 * 1024)
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_TIME_SLICING 1
#define configUSE_MUTEXES 0
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configQUEUE_REGISTRY_SIZE 0
#define configUSE_QUEUE_SETS 0
#define configUSE_NEWLIB_REENTRANT 0
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
/* FreeRTOS+CLI shared response buffer; enough for each command's help text. */
#define configCOMMAND_INT_MAX_OUTPUT_SIZE 256
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_TIMERS 0
#define configUSE_TRACE_FACILITY 0
#define configGENERATE_RUN_TIME_STATS 0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configKERNEL_INTERRUPT_PRIORITY (7u << 5)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (4u << 5)
#define configOVERRIDE_DEFAULT_TICK_CONFIGURATION 0
#define configUSE_TICKLESS_IDLE 0

void cli_example_assert_failed(const char *file, unsigned int line);
#define configASSERT(condition) do { if (!(condition)) cli_example_assert_failed(__FILE__, __LINE__); } while (0)

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelete 0
#define INCLUDE_vTaskPrioritySet 0
#define INCLUDE_uxTaskPriorityGet 0
#define INCLUDE_xTaskGetSchedulerState 0
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif
