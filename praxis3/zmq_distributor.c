#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunker.h"
#include "combine.h"
#include "distributor.h"

#define MAX_MSG_LEN 1500
#define PAYLOAD_MAX 1497

/* ================= BUFFER HELPERS ================= */

void buffer_init(buffer_t *b)
{
    b->cap = 4096;
    b->len = 0;
    b->data = malloc(b->cap);
    if (!b->data) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    b->data[0] = '\0';
}

void buffer_append(buffer_t *b, const char *s)
{
    size_t add = strlen(s);

    while (b->len + add + 1 > b->cap) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
        if (!b->data) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }

    memcpy(b->data + b->len, s, add + 1);
    b->len += add;
}

void buffer_free(buffer_t *b)
{
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

/* ================= RIP HANDLING ================= */

void distributor_send_rip(void **sockets, int worker_count)
{
    for (int i = 0; i < worker_count; i++) {
        zmq_send(sockets[i], "rip", 4, 0);

        char reply[16];
        zmq_recv(sockets[i], reply, sizeof(reply), 0);
    }
}

/* ================= MAIN ================= */

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <input_file> <worker_port_1> [worker_port_2 ...]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int worker_count = argc - 2;
    char **worker_ports = &argv[2];

    /* -------- ZMQ SETUP -------- */

    void *context = zmq_ctx_new();
    if (!context) {
        fprintf(stderr, "Failed to create ZMQ context\n");
        return EXIT_FAILURE;
    }

    void **sockets = calloc(worker_count, sizeof(void *));
    if (!sockets) {
        perror("calloc");
        zmq_ctx_destroy(context);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < worker_count; i++) {
        sockets[i] = zmq_socket(context, ZMQ_REQ);
        if (!sockets[i]) {
            fprintf(stderr, "Failed to create socket\n");
            return EXIT_FAILURE;
        }

        char endpoint[64];
        snprintf(endpoint, sizeof(endpoint),
                 "tcp://localhost:%s", worker_ports[i]);

        if (zmq_connect(sockets[i], endpoint) != 0) {
            fprintf(stderr, "Failed to connect to %s\n", endpoint);
            return EXIT_FAILURE;
        }
    }

    /* -------- CHUNKER INIT -------- */

    chunker_t chunker;
    if (chunker_init(&chunker, filename) != 0) {
        perror("chunker_init");
        return EXIT_FAILURE;
    }

    /* ================= MAP PHASE ================= */

    buffer_t map_results;
    buffer_init(&map_results);

    int worker_idx = 0;
    char *chunk = NULL;

    while (chunker_next(&chunker, &chunk) == 1) {
        char msg[MAX_MSG_LEN];

        strcpy(msg, "map");
        strncat(msg, chunk, PAYLOAD_MAX);

        zmq_send(sockets[worker_idx], msg, strlen(msg) + 1, 0);

        char reply[MAX_MSG_LEN];
        zmq_recv(sockets[worker_idx], reply, sizeof(reply), 0);

        buffer_append(&map_results, reply);

        free(chunk);   /* IMPORTANT: prevent leak */
        worker_idx = (worker_idx + 1) % worker_count;
    }

    chunker_close(&chunker);

    /* ================= REDUCE PHASE ================= */

    buffer_t reduce_results;
    buffer_init(&reduce_results);

    size_t offset = 0;
    worker_idx = 0;

    while (offset < map_results.len) {
        char msg[MAX_MSG_LEN];
        strcpy(msg, "red");

        size_t remaining = map_results.len - offset;
        size_t to_copy = remaining > PAYLOAD_MAX ? PAYLOAD_MAX : remaining;

        strncat(msg, map_results.data + offset, to_copy);
        offset += to_copy;

        zmq_send(sockets[worker_idx], msg, strlen(msg) + 1, 0);

        char reply[MAX_MSG_LEN];
        zmq_recv(sockets[worker_idx], reply, sizeof(reply), 0);

        buffer_append(&reduce_results, reply);

        worker_idx = (worker_idx + 1) % worker_count;
    }

    buffer_free(&map_results);

    /* ================= COMBINE + PRINT ================= */

    combine_and_print(reduce_results.data);
    buffer_free(&reduce_results);

    /* ================= SHUTDOWN ================= */

    distributor_send_rip(sockets, worker_count);

    for (int i = 0; i < worker_count; i++)
        zmq_close(sockets[i]);

    free(sockets);
    zmq_ctx_destroy(context);

    return EXIT_SUCCESS;
}
