#include "cli_command.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *start;
    size_t length;
} token_t;

static int next_token(const char **cursor, token_t *token)
{
    while (**cursor == ' ' || **cursor == '\t') {
        ++*cursor;
    }
    token->start = *cursor;
    while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t') {
        ++*cursor;
    }
    token->length = (size_t)(*cursor - token->start);
    return token->length != 0u;
}

static int matches(token_t token, const char *word)
{
    return token.length == strlen(word) &&
           memcmp(token.start, word, token.length) == 0;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_byte(token_t token, uint8_t *value)
{
    int hi, lo;
    if (token.length != 4u || token.start[0] != '0' || token.start[1] != 'x') {
        return 0;
    }
    hi = hex_digit(token.start[2]);
    lo = hex_digit(token.start[3]);
    if (hi < 0 || lo < 0) return 0;
    *value = (uint8_t)((hi << 4) | lo);
    return 1;
}

cli_parse_result_t cli_parse_command(const char *line,
                                     cli_i2c_request_t *request)
{
    token_t command, token;
    uint8_t args[2u + CLI_I2C_MAX_WRITE_BYTES];
    size_t count = 0;
    cli_i2c_request_t parsed = {0};

    if (line == NULL || request == NULL) return CLI_PARSE_INVALID_ARGUMENT;
    if (!next_token(&line, &command)) return CLI_PARSE_EMPTY;
    if (matches(command, "help")) {
        return next_token(&line, &token) ? CLI_PARSE_ARGUMENT_COUNT : CLI_PARSE_HELP;
    }
    if (matches(command, "i2c_read")) {
        parsed.operation = CLI_I2C_READ;
    } else if (matches(command, "i2c_write")) {
        parsed.operation = CLI_I2C_WRITE;
    } else {
        return CLI_PARSE_UNKNOWN_COMMAND;
    }

    while (next_token(&line, &token)) {
        if (count == sizeof(args)) return CLI_PARSE_ARGUMENT_COUNT;
        if (!hex_byte(token, &args[count])) return CLI_PARSE_INVALID_HEX;
        ++count;
    }
    if (count < 3u || (parsed.operation == CLI_I2C_READ && count != 3u)) {
        return CLI_PARSE_ARGUMENT_COUNT;
    }
    parsed.addr = args[0];
    parsed.reg = args[1];
    if (parsed.operation == CLI_I2C_READ) {
        parsed.len = args[2];
        if (parsed.len == 0u) return CLI_PARSE_ZERO_LENGTH;
    } else {
        parsed.len = (uint8_t)(count - 2u);
        memcpy(parsed.data, &args[2], parsed.len);
    }
    *request = parsed;
    return CLI_PARSE_OK;
}
