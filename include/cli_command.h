#ifndef CLI_COMMAND_H
#define CLI_COMMAND_H

#include <stdint.h>

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

typedef enum {
    CLI_PARSE_OK,
    CLI_PARSE_EMPTY,
    CLI_PARSE_HELP,
    CLI_PARSE_UNKNOWN_COMMAND,
    CLI_PARSE_ARGUMENT_COUNT,
    CLI_PARSE_INVALID_HEX,
    CLI_PARSE_ZERO_LENGTH,
    CLI_PARSE_INVALID_ARGUMENT
} cli_parse_result_t;

/* All numeric tokens must be exactly 0xNN (hex digits are case insensitive).
 * Does not modify line; writes request only on CLI_PARSE_OK. */
cli_parse_result_t cli_parse_command(const char *line,
                                     cli_i2c_request_t *request);

#endif
