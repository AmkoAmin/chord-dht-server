// REP worker: bind to provided ports, reply to "map\0" with "\0", close on "rip\0"
#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

volatile sig_atomic_t running = 1;
static void handle_sigint(int sig) { (void)sig; running = 0; }

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <worker port 1> [<worker port 2> ...]\n", argv[0]);
        return 1;
    }

    int n_ports = argc - 1;
    printf("Starting worker on %d port(s)...\n", n_ports);

    void *context = zmq_ctx_new();
    if (!context) {
        fprintf(stderr, "Failed to create ZMQ context\n");
        return 1;
    }

    void **reps = calloc(n_ports, sizeof(void*));
    if (!reps) {
        fprintf(stderr, "Out of memory\n");
        zmq_ctx_destroy(context);
        return 1;
    }

    for (int i = 0; i < n_ports; ++i) {
        reps[i] = zmq_socket(context, ZMQ_REP);
        if (!reps[i]) {
            fprintf(stderr, "Failed to create REP socket for port %s\n", argv[i+1]);
            reps[i] = NULL;
            continue;
        }
        char endpoint[128];
        snprintf(endpoint, sizeof(endpoint), "tcp://*:%s", argv[i+1]);
        if (zmq_bind(reps[i], endpoint) != 0) {
            fprintf(stderr, "Failed to bind to %s: %s\n", endpoint, zmq_strerror(zmq_errno()));
            zmq_close(reps[i]);
            reps[i] = NULL;
            continue;
        }
        printf("Bound REP socket to %s\n", endpoint);
    }

    signal(SIGINT, handle_sigint);

    zmq_pollitem_t *items = calloc(n_ports, sizeof(zmq_pollitem_t));
    if (!items) {
        fprintf(stderr, "Out of memory (poll items)\n");
        for (int i = 0; i < n_ports; ++i) if (reps[i]) zmq_close(reps[i]);
        free(reps);
        zmq_ctx_destroy(context);
        return 1;
    }

    int active = 0;
    for (int i = 0; i < n_ports; ++i) {
        items[i].socket = reps[i];
        items[i].fd = 0;
        items[i].events = ZMQ_POLLIN;
        items[i].revents = 0;
        if (reps[i]) active++;
    }

    while (running && active > 0) {
        int rc = zmq_poll(items, n_ports, 1000);
        if (rc == -1) {
            if (errno == EINTR) break;
            perror("zmq_poll");
            break;
        }

        for (int i = 0; i < n_ports; ++i) {
            if (!reps[i]) continue;
            if (items[i].revents & ZMQ_POLLIN) {
                char buffer[256];
                int r = zmq_recv(reps[i], buffer, sizeof(buffer), 0);
                if (r < 0) {
                    fprintf(stderr, "recv error on port %s: %s\n", argv[i+1], zmq_strerror(zmq_errno()));
                    continue;
                }
                /* match exact patterns with trailing NUL if present */
                if (r >= 4 && memcmp(buffer, "map\0", 4) == 0) {
                    /* reply with single NUL byte */
                    zmq_send(reps[i], "\0", 1, 0);
                } else if (r >= 4 && memcmp(buffer, "rip\0", 4) == 0) {
                    zmq_send(reps[i], "\0", 1, 0);
                    zmq_close(reps[i]);
                    reps[i] = NULL;
                    items[i].socket = NULL;
                    active--;
                    printf("Closed worker on port %s\n", argv[i+1]);
                } else {
                    /* default reply */
                    zmq_send(reps[i], "\0", 1, 0);
                }
            }
        }
    }

    free(items);
    for (int i = 0; i < n_ports; ++i) if (reps[i]) zmq_close(reps[i]);
    free(reps);
    zmq_ctx_destroy(context);
    return 0;
}
