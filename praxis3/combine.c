#include "combine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------- internal data structure -------- */

typedef struct {
    char *word;
    int count;
} entry_t;

static int cmp_entries(const void *a, const void *b)
{
    const entry_t *ea = a;
    const entry_t *eb = b;
    return strcmp(ea->word, eb->word);
}

/* ---------------------------------------- */

void combine_and_print(const char *reduce_data)
{
    if (!reduce_data || reduce_data[0] == '\0')
        return;

    size_t cap = 128;
    size_t len = 0;
    entry_t *entries = malloc(cap * sizeof(entry_t));

    const char *p = reduce_data;

    /* Parse format: word:count, */
    while (*p) {
        const char *colon = strchr(p, ':');
        const char *comma = strchr(p, ',');

        if (!colon || !comma || colon > comma)
            break;

        size_t word_len = colon - p;
        char *word = strndup(p, word_len);
        int count = atoi(colon + 1);

        /* Deduplicate */
        int found = 0;
        for (size_t i = 0; i < len; i++) {
            if (strcmp(entries[i].word, word) == 0) {
                entries[i].count += count;
                free(word);
                found = 1;
                break;
            }
        }

        if (!found) {
            if (len == cap) {
                cap *= 2;
                entries = realloc(entries, cap * sizeof(entry_t));
            }

            entries[len].word = word;
            entries[len].count = count;
            len++;
        }

        p = comma + 1;
    }

    /* Sort alphabetically */
    qsort(entries, len, sizeof(entry_t), cmp_entries);

    /* Print */
    for (size_t i = 0; i < len; i++) {
        printf("%s %d\n", entries[i].word, entries[i].count);
        free(entries[i].word);
    }

    free(entries);
}
