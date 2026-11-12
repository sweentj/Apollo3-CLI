#ifndef CLI_COMMAND_H
#define CLI_COMMAND_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

#define CLI_I2C_MAX_WRITE_BYTES 16u

typedef enum {
    CLI_I2C_READ,
    CLI_I2C_WRITE
} cli_i2c_operation_t;

/* Queue items own their data; no pointers into the command line. */
typedef struct {
    cli_i2c_operation_t operation;
    uint8_t addr;  /* Passed unchanged; the I2C worker defines address semantics. */
    uint8_t reg;
    uint8_t len;   /* Read: 1..255; write: 1..16. */
    uint8_t data[CLI_I2C_MAX_WRITE_BYTES]; /* Used only for writes. */
} cli_i2c_request_t;

/* Register the I2C commands with FreeRTOS+CLI and bind their destination queue.
 * Call before the scheduler; requests must hold sizeof(cli_i2c_request_t).
 * Successful registrations persist for the application lifetime. Retrying
 * after partial registration does not duplicate commands. Returns pdPASS or
 * pdFAIL. The framework's configured malloc/assert hooks also apply.
 * Only cli_task may process commands; FreeRTOS+CLI is not reentrant. */
BaseType_t cli_commands_register(QueueHandle_t requests);

#endif
