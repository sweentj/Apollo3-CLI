#ifndef CLI_TASK_H
#define CLI_TASK_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cli_command.h"

#define CLI_TASK_STACK_WORDS 512u
#define CLI_TASK_PRIORITY (tskIDLE_PRIORITY + 1u)

typedef enum {
    CLI_START_OK,
    CLI_START_INVALID_QUEUE,
    CLI_START_ALREADY_STARTED,
    CLI_START_NO_MEMORY,
    CLI_START_UART_ERROR
} cli_start_result_t;

/* Call once before vTaskStartScheduler(), after board initialization.
 * requests must be created with sizeof(cli_i2c_request_t) and must remain alive.
 * Owns UART0 and am_uart_isr. No other task may access this UART.
 * On failure UART/RX resources are released and a retry is allowed. Framework
 * command registrations persist for the application lifetime. */
cli_start_result_t cli_start(QueueHandle_t requests);
void cli_task(void *argument);

/* Debugger-visible HAL error; a runtime TX failure suspends the CLI task. */
extern volatile uint32_t cli_last_hal_error;

#endif
