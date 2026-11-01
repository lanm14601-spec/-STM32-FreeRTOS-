#ifndef CONSOLE_PROTOCOL_H
#define CONSOLE_PROTOCOL_H
#include <stdint.h>
#include "console_config.h"
typedef enum { CMD_HELP = 0, CMD_STATUS, CMD_PAGE, CMD_LOG, CMD_PERIOD, CMD_ERROR } ConsoleKind;
typedef enum { CONSOLE_OK = 0, CONSOLE_UNKNOWN, CONSOLE_SYNTAX, CONSOLE_RANGE,
               CONSOLE_LINE_LONG, CONSOLE_BAD_CHAR, CONSOLE_RX_LOSS } ConsoleError;
typedef struct {
    uint32_t id; /* Assigned by ConsoleTask to each completed command/error. */
    ConsoleKind kind;
    uint32_t value;
    ConsoleError error;
} ConsoleCommand;
typedef struct {
    char line[CONSOLE_LINE_CAPACITY];
    uint16_t length;
    ConsoleError discard_error;
} ConsoleParser;
void ConsoleParser_Init(ConsoleParser *parser);
/* Drop the partial line; report exactly one error at the next terminator. */
void ConsoleParser_Lost(ConsoleParser *parser);
/* Returns 1 when out is complete. CR, LF and CRLF accepted; empty lines ignored.
 * ASCII case-insensitive. Backspace/DEL edit only a not-yet-discarded line.
 */
uint8_t ConsoleParser_Feed(ConsoleParser *parser, uint8_t byte, ConsoleCommand *out);
const char *Console_ErrorName(ConsoleError error);
#endif
