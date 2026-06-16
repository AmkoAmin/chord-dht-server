#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

#include <stddef.h>

/*
 * Buffer structure used to collect map and reduce outputs
 * (opaque to other modules except combine.c)
 */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buffer_t;

/*
 * Initialize a dynamic buffer
 */
void buffer_init(buffer_t *b);

/*
 * Append a NUL-terminated string to buffer
 */
void buffer_append(buffer_t *b, const char *s);

/*
 * Free buffer memory
 */
void buffer_free(buffer_t *b);

/*
 * Send "rip" to all workers and wait for replies
 */
void distributor_send_rip(void **sockets, int worker_count);

#endif /* DISTRIBUTOR_H */
