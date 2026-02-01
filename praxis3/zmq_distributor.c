#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "chunker.h"
#include "combine.h"
#include "distributor.h"

#define MAX_MSG_LEN 1500
#define HEADER_LEN 6
#define PAYLOAD_MAX (MAX_MSG_LEN - HEADER_LEN - 1)  /* max payload that fits: 1500 - 6 - 1 (NUL) */

static size_t find_safe_split(const char *data, size_t max_len) {
    if (max_len == 0) return 0;
    for (size_t i = max_len; i > 0; --i) {
        if (i >= 1) {
            char prev = data[i - 1];
            char curr = data[i];
            if (isdigit((unsigned char)prev) && isalpha((unsigned char)curr)) {
                return i;
            }
        }
    }
    return max_len;
}

/* ================= MESSAGE BUILDER ================= */

/**
 * build_msg - Build a protocol message: type (3 bytes) + len3 (3 ASCII) + payload + NUL
 * @out: output buffer (must be at least MAX_MSG_LEN)
 * @type: 3-byte type (e.g., "map", "red", "rip")
 * @msg_len: desired max message length (will be capped to MAX_MSG_LEN)
 * @payload: NUL-terminated payload string (may be NULL or empty)
 * @return: total message length including NUL terminator
 */
static size_t build_msg(char out[MAX_MSG_LEN], const char type[3],
                        int msg_len, const char *payload)
{
    if (msg_len <= 0 || msg_len > MAX_MSG_LEN) {
        msg_len = MAX_MSG_LEN;
    }

    /* Write type */
    out[0] = type[0];
    out[1] = type[1];
    out[2] = type[2];

    /* Write msg_len as 3 ASCII digits */
    out[3] = '0' + (msg_len / 100) % 10;
    out[4] = '0' + (msg_len / 10) % 10;
    out[5] = '0' + msg_len % 10;

    /* Calculate max payload length (accounting for header + NUL) */
    int max_payload = msg_len - HEADER_LEN - 1;
    if (max_payload < 1) {
        max_payload = 1;  /* at least room for '\0' */
    }

    /* Copy payload, truncate if needed */
    size_t payload_len = 0;
    if (payload) {
        payload_len = strlen(payload);
        if (payload_len > (size_t)max_payload) {
            payload_len = max_payload;
        }
        memcpy(out + HEADER_LEN, payload, payload_len);
    }

    /* Add NUL terminator */
    out[HEADER_LEN + payload_len] = '\0';

    return HEADER_LEN + payload_len + 1;
}

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
        /* Use legacy text protocol: "rip\0" */
        zmq_send(sockets[i], "rip\0", 4, 0);

        char reply[MAX_MSG_LEN];
        int reply_len = zmq_recv(sockets[i], reply, sizeof(reply) - 1, 0);
        (void)reply_len;  /* unused, just drain the reply */
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
        /* Use legacy text protocol: "map" + payload + NUL */
        snprintf(msg, sizeof(msg), "map%s", chunk);

        zmq_send(sockets[worker_idx], msg, strlen(msg) + 1, 0);

        char reply[MAX_MSG_LEN];
        int reply_len = zmq_recv(sockets[worker_idx], reply, sizeof(reply) - 1, 0);
        if (reply_len > 0) {
            reply[reply_len] = '\0';
            buffer_append(&map_results, reply);
        }

        /* chunk is pointer to internal chunker buffer, do not free */
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

        size_t remaining = map_results.len - offset;
        size_t to_copy = remaining > (size_t)PAYLOAD_MAX ? (size_t)PAYLOAD_MAX : remaining;

        if (remaining > (size_t)PAYLOAD_MAX) {
            size_t safe = find_safe_split(map_results.data + offset, to_copy);
            if (safe > 0 && safe < to_copy) {
                to_copy = safe;
            }
        }

        /* Extract chunk from map_results (may not be NUL-terminated at boundary) */
        char payload_buf[PAYLOAD_MAX + 1];
        memcpy(payload_buf, map_results.data + offset, to_copy);
        payload_buf[to_copy] = '\0';

        /* Use legacy text protocol: "red" + payload + NUL */
        snprintf(msg, sizeof(msg), "red%s", payload_buf);
        offset += to_copy;

        zmq_send(sockets[worker_idx], msg, strlen(msg) + 1, 0);

        char reply[MAX_MSG_LEN];
        int reply_len = zmq_recv(sockets[worker_idx], reply, sizeof(reply) - 1, 0);
        if (reply_len > 0) {
            reply[reply_len] = '\0';
            buffer_append(&reduce_results, reply);
        }

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
