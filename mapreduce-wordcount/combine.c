#include "combine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------- internal data structure -------- */

typedef struct {
    char *word;
    long count;
} entry_t;

static int cmp_by_count_desc_word_asc(const void *a, const void *b)
{
    const entry_t *ea = (const entry_t*)a;
    const entry_t *eb = (const entry_t*)b;

    if (ea->count < eb->count) return 1;
    if (ea->count > eb->count) return -1;
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
    if (!entries) {
        perror("malloc");
        return;
    }

    const char *p = reduce_data;

    /* Parse format: word1count1word2count2...
     * where word is [a-zA-Z]+ and count is [0-9]+
     */
    while (*p && *p != '\0') {
        /* Skip any non-letter */
        while (*p && !isalpha((unsigned char)*p)) {
            p++;
        }
        
        if (!*p || *p == '\0')
            break;

        /* Read word */
        const char *word_start = p;
        while (*p && isalpha((unsigned char)*p)) {
            p++;
        }
        size_t word_len = p - word_start;
        
        if (word_len == 0)
            break;
        
        char *word = malloc(word_len + 1);
        if (!word) {
            perror("malloc");
            break;
        }
        strncpy(word, word_start, word_len);
        word[word_len] = '\0';

        /* Read count */
        const char *count_start = p;
        long count = 0;
        while (*p && isdigit((unsigned char)*p)) {
            count = count * 10 + (*p - '0');
            p++;
        }

        /* Check if count was actually parsed */
        if (p == count_start) {
            /* No digits found, skip this entry */
            free(word);
            continue;
        }

        /* Accumulate into hashmap or array */
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
                if (!entries) {
                    perror("realloc");
                    free(word);
                    return;
                }
            }

            entries[len].word = word;
            entries[len].count = count;
            len++;
        }
    }

    /* Sort by count descending, then word ascending */
    qsort(entries, len, sizeof(entry_t), cmp_by_count_desc_word_asc);

    /* Print CSV format: word,frequency header + word,count lines */
    printf("word,frequency\n");
    for (size_t i = 0; i < len; i++) {
        printf("%s,%ld\n", entries[i].word, entries[i].count);
        free(entries[i].word);
    }

    free(entries);
}
