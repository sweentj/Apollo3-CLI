#ifndef CLI_LINE_H
#define CLI_LINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Longest canonical write command is 99 characters, plus NUL. */
#define CLI_LINE_CAPACITY 128u

typedef struct {
    char text[CLI_LINE_CAPACITY];
    size_t length;
    bool discard;
    bool after_cr;
} cli_line_t;

typedef enum {
    CLI_LINE_NONE,
    CLI_LINE_ECHO,
    CLI_LINE_ERASE,
    CLI_LINE_READY,
    CLI_LINE_REJECTED,
    CLI_LINE_CANCELLED
} cli_line_event_t;

void cli_line_init(cli_line_t *line);
/* Reject the entire damaged line through the next CR/LF. */
void cli_line_invalidate(cli_line_t *line);
/* READY text remains valid until the next feed; CRLF produces one READY. */
cli_line_event_t cli_line_feed(cli_line_t *line, uint8_t byte);

/* Trim leading/trailing whitespace and collapse spaces/tabs in place for
 * FreeRTOS+CLI, which recognizes spaces only. Input must be NUL-terminated. */
void cli_line_normalize(char *text);

#endif
