#include "cli_command.h"
#include "FreeRTOS_CLI.h"

#include <string.h>

static QueueHandle_t request_queue;

static BaseType_t read_command(char *output, size_t capacity, const char *command);
static BaseType_t write_command(char *output, size_t capacity, const char *command);

static const CLI_Command_Definition_t commands[] = {
    {
        "i2c_read",
        "i2c_read <addr> <reg> <len>:\r\n"
        " All values: 0xNN. Read length: 0x01..0xFF.\r\n"
        " OK queued is not I2C completion.\r\n\r\n",
        read_command,
        3
    },
    {
        "i2c_write",
        "i2c_write <addr> <reg> <data1> ... <data16>:\r\n"
        " All values: 0xNN. Write: 1..16 bytes.\r\n"
        " Backspace/Delete edit; Ctrl-C cancels input.\r\n"
        " OK queued is not I2C completion.\r\n\r\n",
        write_command,
        -1 /* Variable count: the callback validates the 1..16 byte limit. */
    }
};

BaseType_t cli_commands_register(QueueHandle_t requests)
{
    static size_t registered;
    if (requests == NULL) return pdFAIL;
    /* FreeRTOS+CLI has no unregister API. Keep successful registrations if a
     * later allocation/startup fails, and resume at the next command on retry. */
    while (registered < sizeof(commands) / sizeof(commands[0])) {
        if (FreeRTOS_CLIRegisterCommand(&commands[registered]) != pdPASS) return pdFAIL;
        ++registered;
    }
    request_queue = requests;
    return pdPASS;
}

static BaseType_t reply(char *output, size_t capacity, const char *text)
{
    if (capacity != 0u) {
        size_t length = strlen(text);
        if (length >= capacity) length = capacity - 1u;
        memcpy(output, text, length);
        output[length] = '\0';
    }
    /* Each I2C callback completes in one call; never queue it twice. */
    return pdFALSE;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static BaseType_t parameter_byte(const char *command, UBaseType_t index, uint8_t *value)
{
    BaseType_t length;
    const char *parameter = FreeRTOS_CLIGetParameter(command, index, &length);
    int hi, lo;
    /* Framework parameters are spans, not NUL-terminated strings. */
    if (parameter == NULL || length != 4 || parameter[0] != '0' || parameter[1] != 'x') {
        return pdFAIL;
    }
    hi = hex_digit(parameter[2]);
    lo = hex_digit(parameter[3]);
    if (hi < 0 || lo < 0) return pdFAIL;
    *value = (uint8_t)((hi << 4) | lo);
    return pdPASS;
}

static BaseType_t queue_request(char *output, size_t capacity, const cli_i2c_request_t *request)
{
    if (request_queue == NULL) return reply(output, capacity, "ERR I2C queue unavailable\r\n");
    /* Copy all arguments, including write data, without blocking UART input. */
    if (xQueueSend(request_queue, request, 0) != pdPASS) {
        return reply(output, capacity, "ERR I2C queue full; command not queued\r\n");
    }
    return reply(output, capacity, "OK queued\r\n");
}

static BaseType_t read_command(char *output, size_t capacity, const char *command)
{
    cli_i2c_request_t request = {0};
    request.operation = CLI_I2C_READ;
    /* FreeRTOS+CLI enforces exactly three parameters before calling us. */
    if (parameter_byte(command, 1, &request.addr) != pdPASS ||
        parameter_byte(command, 2, &request.reg) != pdPASS ||
        parameter_byte(command, 3, &request.len) != pdPASS) {
        return reply(output, capacity, "ERR expected byte in exact 0xNN format\r\n");
    }
    if (request.len == 0u) {
        return reply(output, capacity, "ERR read length must be 0x01..0xFF\r\n");
    }
    return queue_request(output, capacity, &request);
}

static BaseType_t write_command(char *output, size_t capacity, const char *command)
{
    cli_i2c_request_t request = {0};
    BaseType_t length;
    UBaseType_t count = 0;
    request.operation = CLI_I2C_WRITE;

    /* Stop as soon as a nineteenth parameter is found (2 address/register +
     * at most 16 data bytes). Do not truncate or enqueue a partial write. */
    while (FreeRTOS_CLIGetParameter(command, count + 1u, &length) != NULL) {
        if (++count > 2u + CLI_I2C_MAX_WRITE_BYTES) {
            return reply(output, capacity, "ERR argument count; type help\r\n");
        }
    }
    if (count < 3u) return reply(output, capacity, "ERR argument count; type help\r\n");
    if (parameter_byte(command, 1, &request.addr) != pdPASS ||
        parameter_byte(command, 2, &request.reg) != pdPASS) {
        return reply(output, capacity, "ERR expected byte in exact 0xNN format\r\n");
    }
    request.len = (uint8_t)(count - 2u);
    for (UBaseType_t i = 0; i < request.len; ++i) {
        if (parameter_byte(command, i + 3u, &request.data[i]) != pdPASS) {
            return reply(output, capacity, "ERR expected byte in exact 0xNN format\r\n");
        }
    }
    return queue_request(output, capacity, &request);
}
