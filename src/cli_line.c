#include "cli_line.h"

#include <string.h>

void cli_line_init(cli_line_t *line)
{
    memset(line, 0, sizeof(*line));
}

void cli_line_normalize(char *text)
{
    char *read = text;
    char *write = text;
    bool space = false;
    while (*read != '\0') {
        if (*read == ' ' || *read == '\t') {
            space = write != text;
        } else {
            if (space) *write++ = ' ';
            *write++ = *read;
            space = false;
        }
        ++read;
    }
    *write = '\0';
}

void cli_line_invalidate(cli_line_t *line)
{
    line->length = 0;
    line->discard = true;
    line->after_cr = false;
}

cli_line_event_t cli_line_feed(cli_line_t *line, uint8_t byte)
{
    if (byte == '\n' && line->after_cr) {
        line->after_cr = false;
        return CLI_LINE_NONE;
    }
    line->after_cr = byte == '\r';
    if (byte == '\r' || byte == '\n') {
        cli_line_event_t result = line->discard ? CLI_LINE_REJECTED : CLI_LINE_READY;
        line->text[line->length] = '\0';
        line->length = 0;
        line->discard = false;
        return result;
    }
    if (byte == 0x03u) { /* Ctrl-C */
        line->length = 0;
        line->discard = false;
        return CLI_LINE_CANCELLED;
    }
    if (line->discard) return CLI_LINE_NONE;
    if (byte == '\b' || byte == 0x7fu) {
        if (line->length == 0u) return CLI_LINE_NONE;
        --line->length;
        return CLI_LINE_ERASE;
    }
    if ((byte < 0x20u && byte != '\t') || byte > 0x7eu ||
        line->length == sizeof(line->text) - 1u) {
        /* Never execute a truncated command or silently delete a bad byte. */
        cli_line_invalidate(line);
        return CLI_LINE_NONE;
    }
    line->text[line->length++] = (char)byte;
    return CLI_LINE_ECHO;
}
