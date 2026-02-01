#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunker.h"
#include "combine.h"
#include "distributor.h"

#define MAX_MSG_LEN 1500
#define PAYLOAD_MAX 1497

/* ---------------- buffer helpers ---------------- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} buffer_t;

static void buffer_init(buffer_t *b)
{
    b->cap = 4096;
    b->len = 0;
    b->data = malloc(b->cap);
    b->data[0] = '\0';
}

static void buffer_append(buffer_t *b, const char *s)
{
    size_t add = strlen(s);

    while (b->len + add + 1 > b->cap) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }

    memcpy(b->data + b->len, s, add + 1);
    b->len += add;
}

static void buffer_free(buffer_t *b)
{
    free(b->data);
}

/* ------------------------------------------------ */

int run_distributor(
    const char *filename,
    int worker_count,
    char **worker_ports
) {
    /* ZMQ context */
    void *context = zmq_ctx_new();

    /* Sockets */
    void **sockets = calloc(worker_count, sizeof(void *));
    for (int i = 0; i < worker_count; i++) {
        sockets[i] = zmq_socket(context, ZMQ_REQ);

        char endpoint[64];
        snprintf(endpoint, sizeof(endpoint),
                 "tcp://localhost:%s", worker_ports[i]);

        zmq_connect(sockets[i], endpoint);
    }

    /* ---------- MAP PHASE ---------- */

    chunker_t chunker;
    if (chunker_init(&chunker, filename) != 0) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    buffer_t map_results;
    buffer_init(&map_results);

    char *chunk;
    int w = 0;

    while (chunker_next(&chunker, &chunk) == 1) {
        char msg[MAX_MSG_LEN];
        strcpy(msg, "map");
        strncat(msg, chunk, PAYLOAD_MAX);

        zmq_send(sockets[w], msg, strlen(msg) + 1, 0);

        char reply[MAX_MSG_LEN];
        zmq_recv(sockets[w], reply, sizeof(reply), 0);

        buffer_append(&map_results, reply);

        w = (w + 1) % worker_count;
    }

    chunker_close(&chunker);

    /* ---------- REDUCE PHASE ---------- */

    buffer_t reduce_results;
    buffer_init(&reduce_results);

    size_t offset = 0;
    w = 0;

    while (offset < map_results.len) {
        char msg[MAX_MSG_LEN];
        strcpy(msg, "red");

        size_t remaining = map_results.len - offset;
        size_t copy = remaining > PAYLOAD_MAX ? PAYLOAD_MAX : remaining;

        strncat(msg, map_results.data + offset, copy);
        offset += copy;

        zmq_send(sockets[w], msg, strlen(msg) + 1, 0);

        char reply[MAX_MSG_LEN];
        zmq_recv(sockets[w], reply, sizeof(reply), 0);

        buffer_append(&reduce_results, reply);

        w = (w + 1) % worker_count;
    }

    buffer_free(&map_results);

    /* ---------- FINAL COMBINE ---------- */

    combine_and_print(reduce_results.data);
    buffer_free(&reduce_results);

    /* ---------- RIP ---------- */

    for (int i = 0; i < worker_count; i++) {
        zmq_send(sockets[i], "rip", 4, 0);

        char reply[MAX_MSG_LEN];
        zmq_recv(sockets[i], reply, sizeof(reply), 0);
    }

    /* Cleanup */
    for (int i = 0; i < worker_count; i++)
        zmq_close(sockets[i]);

    free(sockets);
    zmq_ctx_destroy(context);

    return EXIT_SUCCESS;
}

/* main */

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <file> <worker1> [worker2 ...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    return run_distributor(
        argv[1],
        argc - 2,
        &argv[2]
    );
}
