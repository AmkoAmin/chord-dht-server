#include "chunker.h"
#include <ctype.h>
#include <string.h>

int chunker_init(chunker_t *c, const char *filename)
{
    c->file = fopen(filename, "r");
    if (!c->file)
        return -1;

    return 0;
}

static int is_word_char(int ch)
{
    return isalpha((unsigned char)ch);
}

int chunker_next(chunker_t *c, char **out_chunk)
{
    if (!c->file)
        return -1;

    int pos = 0;
    int ch;
    int last_safe_split = -1;

    while (pos < CHUNK_MAX_SIZE) {
        ch = fgetc(c->file);
        if (ch == EOF)
            break;

        c->buffer[pos++] = (char)ch;

        if (!is_word_char(ch)) {
            last_safe_split = pos;
        }
    }

    if (pos == 0)
        return 0; // EOF

    /* If buffer filled and last char was mid-word, rewind */
    if (pos == CHUNK_MAX_SIZE && last_safe_split != -1) {
        int rewind_bytes = pos - last_safe_split;
        fseek(c->file, -rewind_bytes, SEEK_CUR);
        pos = last_safe_split;
    }

    c->buffer[pos] = '\0';
    *out_chunk = c->buffer;
    return 1;
}

void chunker_close(chunker_t *c)
{
    if (c->file) {
        fclose(c->file);
        c->file = NULL;
    }
}
