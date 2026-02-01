#ifndef CHUNKER_H
#define CHUNKER_H

#include <stdio.h>

#define CHUNK_MAX_SIZE 1497

typedef struct {
    FILE *file;
    char buffer[CHUNK_MAX_SIZE + 1];
} chunker_t;

/* Initialize chunker with open file */
int chunker_init(chunker_t *c, const char *filename);

/* Read next chunk
 * returns:
 *   1  → chunk read
 *   0  → EOF
 *  -1  → error
 */
int chunker_next(chunker_t *c, char **out_chunk);

/* Cleanup */
void chunker_close(chunker_t *c);

#endif
