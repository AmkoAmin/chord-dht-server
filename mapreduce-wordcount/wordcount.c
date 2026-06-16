#include "wordcount.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Helper: Check if character is ASCII letter */
static int is_alpha_ascii(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* Helper: Convert ASCII character to lowercase */
static char tolower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - ('A' - 'a');
    }
    return c;
}

/*
 * wc_tokenize_and_count - Tokenize text and count words
 */
void wc_tokenize_and_count(HashMap *hm, const char *text) {
    if (hm == NULL || text == NULL) {
        return;
    }

    /* Buffer for accumulating current word */
    char word[256];
    size_t word_len = 0;

    for (size_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];

        if (is_alpha_ascii(c)) {
            /* Add to current word (with safety check) */
            if (word_len < sizeof(word) - 1) {
                word[word_len++] = tolower_ascii(c);
            }
        } else {
            /* Non-letter: end current word if any */
            if (word_len > 0) {
                word[word_len] = '\0';
                hm_put_inc(hm, word, 1);
                word_len = 0;
            }
        }
    }

    /* Don't forget final word if text ends with letter */
    if (word_len > 0) {
        word[word_len] = '\0';
        hm_put_inc(hm, word, 1);
    }
}

/*
 * Callback context for building output string in map format
 */
typedef struct {
    char *buffer;
    size_t pos;          /* current position in buffer */
    size_t max_len;      /* maximum allowed (including NUL) */
    int overflow;        /* flag if we hit limit */
} map_format_ctx_t;

/*
 * Callback for hm_foreach in map format
 * Appends: word + '1' repeated count times
 */
static void append_map_entry(const char *key, int count, void *ctx) {
    map_format_ctx_t *mc = (map_format_ctx_t *)ctx;

    if (mc->overflow) {
        return;
    }

    if (key == NULL) {
        key = "";
    }

    /* Calculate space needed: strlen(key) + count + 1 (NUL) */
    size_t key_len = strlen(key);
    size_t needed = key_len + count + 1;  /* +1 for NUL */

    if (mc->pos + needed > mc->max_len) {
        /* Would overflow: truncate and mark */
        mc->overflow = 1;
        return;
    }

    /* Copy key */
    memcpy(mc->buffer + mc->pos, key, key_len);
    mc->pos += key_len;

    /* Add '1' repeated count times */
    for (int i = 0; i < count; i++) {
        mc->buffer[mc->pos++] = '1';
    }
}

/*
 * wc_to_map_format - Convert to map format
 */
char *wc_to_map_format(const HashMap *hm, size_t msg_len) {
    if (msg_len <= 1) {
        char *empty = (char *)malloc(1);
        if (empty != NULL) {
            empty[0] = '\0';
        }
        return empty;
    }

    char *buffer = (char *)malloc(msg_len);
    if (buffer == NULL) {
        return NULL;
    }

    map_format_ctx_t ctx;
    ctx.buffer = buffer;
    ctx.pos = 0;
    ctx.max_len = msg_len;
    ctx.overflow = 0;

    /* Iterate hashmap and build string */
    hm_foreach((HashMap *)hm, append_map_entry, &ctx);

    /* NUL-terminate */
    buffer[ctx.pos] = '\0';

    return buffer;
}

/*
 * wc_from_map_format - Parse map format
 */
/*
 * wc_from_map_format - Parse map format
 *
 * Examples:
 * - Input: "the11example11text1" -> the=2, example=2, text=1
 * - Input with junk: "the11,example11!!text1" -> same result (junk skipped)
 * - Input starting with ones: "111the11" -> the=2 (leading '1's ignored)
 */
HashMap *wc_from_map_format(const char *payload) {
    HashMap *hm = hm_create(1024);
    if (hm == NULL) {
        return NULL;
    }

    if (payload == NULL) {
        return hm;
    }

    /* State machine: START -> reading word -> counting ones -> finalize */
    enum { STATE_START, STATE_IN_WORD, STATE_IN_ONES } state = STATE_START;

    char word[256];
    size_t word_len = 0;
    int count = 0;

    for (size_t i = 0; payload[i] != '\0'; i++) {
        char c = payload[i];

        if (state == STATE_START) {
            /* Looking for start of a new word */
            if (is_alpha_ascii(c)) {
                word_len = 0;
                if (word_len < sizeof(word) - 1) {
                    word[word_len++] = tolower_ascii(c);
                }
                state = STATE_IN_WORD;
            }
            /* Skip everything else (including stray '1's) */
        }
        else if (state == STATE_IN_WORD) {
            /* Accumulating word letters */
            if (is_alpha_ascii(c)) {
                if (word_len < sizeof(word) - 1) {
                    word[word_len++] = tolower_ascii(c);
                }
            }
            else if (c == '1') {
                /* Transition to counting ones */
                count = 1;
                state = STATE_IN_ONES;
            }
            else {
                /* Unexpected character: discard incomplete word */
                state = STATE_START;
                word_len = 0;
            }
        }
        else {
            /* STATE_IN_ONES: counting '1's */
            if (c == '1') {
                count++;
            }
            else if (is_alpha_ascii(c)) {
                /* Finalize current pair */
                if (word_len > 0 && count > 0) {
                    word[word_len] = '\0';
                    hm_put_inc(hm, word, count);
                }
                /* Start new word */
                word_len = 0;
                if (word_len < sizeof(word) - 1) {
                    word[word_len++] = tolower_ascii(c);
                }
                state = STATE_IN_WORD;
                count = 0;
            }
            else {
                /* Non-letter, non-'1': finalize and go to START */
                if (word_len > 0 && count > 0) {
                    word[word_len] = '\0';
                    hm_put_inc(hm, word, count);
                }
                state = STATE_START;
                word_len = 0;
                count = 0;
            }
        }
    }

    /* Don't forget final pair if string ends in STATE_IN_ONES */
    if (state == STATE_IN_ONES && word_len > 0 && count > 0) {
        word[word_len] = '\0';
        hm_put_inc(hm, word, count);
    }

    return hm;
}

/*
 * Callback context for building output string in reduce format
 */
typedef struct {
    char *buffer;
    size_t pos;
    size_t max_len;
    int overflow;
} reduce_format_ctx_t;

/*
 * Callback for hm_foreach in reduce format
 * Appends: word + decimal count
 */
static void append_reduce_entry(const char *key, int count, void *ctx) {
    reduce_format_ctx_t *rc = (reduce_format_ctx_t *)ctx;

    if (rc->overflow) {
        return;
    }

    if (key == NULL) {
        key = "";
    }

    size_t key_len = strlen(key);

    /* Format count as decimal string in temp buffer */
    char count_str[32];
    int count_len = snprintf(count_str, sizeof(count_str), "%d", count);
    if (count_len < 0 || count_len >= (int)sizeof(count_str)) {
        rc->overflow = 1;
        return;
    }

    /* Check if we have space for key + count + NUL */
    size_t needed = key_len + count_len + 1;  /* +1 for NUL */
    if (rc->pos + needed > rc->max_len) {
        rc->overflow = 1;
        return;
    }

    /* Copy key */
    memcpy(rc->buffer + rc->pos, key, key_len);
    rc->pos += key_len;

    /* Copy decimal count */
    memcpy(rc->buffer + rc->pos, count_str, count_len);
    rc->pos += count_len;
}

/*
 * wc_to_reduce_format - Convert to reduce format
 */
char *wc_to_reduce_format(const HashMap *hm, size_t msg_len) {
    if (msg_len <= 1) {
        char *empty = (char *)malloc(1);
        if (empty != NULL) {
            empty[0] = '\0';
        }
        return empty;
    }

    char *buffer = (char *)malloc(msg_len);
    if (buffer == NULL) {
        return NULL;
    }

    reduce_format_ctx_t ctx;
    ctx.buffer = buffer;
    ctx.pos = 0;
    ctx.max_len = msg_len;
    ctx.overflow = 0;

    /* Iterate hashmap and build string */
    hm_foreach((HashMap *)hm, append_reduce_entry, &ctx);

    /* NUL-terminate */
    buffer[ctx.pos] = '\0';

    return buffer;
}
