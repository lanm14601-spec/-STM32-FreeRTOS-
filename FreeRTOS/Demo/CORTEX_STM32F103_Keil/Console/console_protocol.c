#include <string.h>
#include "console_protocol.h"

void ConsoleParser_Init(ConsoleParser *parser)
{
    if (parser != 0) memset(parser, 0, sizeof(*parser));
}

void ConsoleParser_Lost(ConsoleParser *parser)
{
    if (parser == 0) return;
    parser->length = 0U;
    parser->discard_error = CONSOLE_RX_LOSS;
}

static uint8_t Space(char c) { return (uint8_t)((c == ' ') || (c == '\t')); }

static uint8_t Number(const char *text, uint32_t *out)
{
    uint32_t value = 0U;
    uint32_t digit;
    if (*text == '\0') return 0U;
    while (*text != '\0')
    {
        if ((*text < '0') || (*text > '9')) return 0U;
        digit = (uint32_t)(*text - '0');
        if (value > (UINT32_MAX - digit) / 10U) return 0U;
        value = value * 10U + digit;
        ++text;
    }
    *out = value;
    return 1U;
}

static uint8_t Parse(char *line, ConsoleCommand *out)
{
    char *tokens[3];
    unsigned int count = 0U;
    char *p = line;
    uint32_t value;
    while (*p != '\0')
    {
        while (Space(*p)) ++p;
        if (*p == '\0') break;
        if (count == 3U) break;
        tokens[count++] = p;
        while ((*p != '\0') && !Space(*p)) ++p;
        if (*p != '\0') *p++ = '\0';
    }
    if (count == 0U) return 0U;
    memset(out, 0, sizeof(*out));
    out->kind = CMD_ERROR;
    out->error = CONSOLE_SYNTAX;
    if ((strcmp(tokens[0], "HELP") == 0) || (strcmp(tokens[0], "STATUS") == 0) ||
        (strcmp(tokens[0], "HOME") == 0))
    {
        if (count != 1U) return 1U;
        if (strcmp(tokens[0], "HELP") == 0) out->kind = CMD_HELP;
        else if (strcmp(tokens[0], "STATUS") == 0) out->kind = CMD_STATUS;
        else { out->kind = CMD_PAGE; out->value = 1U; }
    }
    else if (strcmp(tokens[0], "LOG") == 0)
    {
        if (count != 2U) return 1U;
        if (strcmp(tokens[1], "ON") == 0) out->value = 1U;
        else if (strcmp(tokens[1], "OFF") == 0) out->value = 0U;
        else return 1U;
        out->kind = CMD_LOG;
    }
    else if ((strcmp(tokens[0], "PAGE") == 0) || (strcmp(tokens[0], "PERIOD") == 0))
    {
        if (count != 2U) return 1U;
        if (!Number(tokens[1], &value)) { out->error = CONSOLE_RANGE; return 1U; }
        if (strcmp(tokens[0], "PAGE") == 0)
        {
            if ((value < 1U) || (value > 3U)) { out->error = CONSOLE_RANGE; return 1U; }
            out->kind = CMD_PAGE;
        }
        else
        {
            if ((value < CONSOLE_MIN_PERIOD_MS) || (value > CONSOLE_MAX_PERIOD_MS))
                { out->error = CONSOLE_RANGE; return 1U; }
            out->kind = CMD_PERIOD;
        }
        out->value = value;
    }
    else { out->error = CONSOLE_UNKNOWN; return 1U; }
    out->error = CONSOLE_OK;
    return 1U;
}

uint8_t ConsoleParser_Feed(ConsoleParser *parser, uint8_t byte, ConsoleCommand *out)
{
    uint8_t ready;
    if ((parser == 0) || (out == 0)) return 0U;
    if ((byte == '\r') || (byte == '\n'))
    {
        if (parser->discard_error != CONSOLE_OK)
        {
            memset(out, 0, sizeof(*out));
            out->kind = CMD_ERROR;
            out->error = parser->discard_error;
            ready = 1U;
        }
        else
        {
            parser->line[parser->length] = '\0';
            ready = Parse(parser->line, out);
        }
        ConsoleParser_Init(parser);
        return ready;
    }
    if (parser->discard_error != CONSOLE_OK) return 0U;
    if ((byte == 8U) || (byte == 127U))
    {
        if (parser->length != 0U) --parser->length;
        return 0U;
    }
    if (((byte < 32U) && (byte != '\t')) || (byte > 126U))
    {
        parser->discard_error = CONSOLE_BAD_CHAR;
        return 0U;
    }
    if (parser->length >= CONSOLE_LINE_CAPACITY - 1U)
    {
        parser->discard_error = CONSOLE_LINE_LONG;
        return 0U;
    }
    if ((byte >= 'a') && (byte <= 'z')) byte = (uint8_t)(byte - 'a' + 'A');
    parser->line[parser->length++] = (char)byte;
    return 0U;
}

const char *Console_ErrorName(ConsoleError error)
{
    switch (error)
    {
    case CONSOLE_OK: return "OK";
    case CONSOLE_UNKNOWN: return "UNKNOWN_COMMAND";
    case CONSOLE_SYNTAX: return "SYNTAX";
    case CONSOLE_RANGE: return "RANGE";
    case CONSOLE_LINE_LONG: return "LINE_TOO_LONG";
    case CONSOLE_BAD_CHAR: return "ASCII_ONLY";
    case CONSOLE_RX_LOSS: return "RX_LOSS_RETRY";
    default: return "UNKNOWN_ERROR";
    }
}
