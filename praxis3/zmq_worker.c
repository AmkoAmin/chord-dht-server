#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include "wordcount.h"

#define MAX_MSG_LEN 1500
#define HEADER_LEN 6
#define RECV_BUFFER_SIZE 2048

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static int is_digit_ascii(char c) {
    return (c >= '0' && c <= '9');
}

static size_t parse_msg_len(const char *p) {
    if (p == NULL || !is_digit_ascii(p[0]) || !is_digit_ascii(p[1]) || !is_digit_ascii(p[2])) {
        return MAX_MSG_LEN;
    }
    size_t val = (size_t)((p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0'));
    if (val == 0 || val > MAX_MSG_LEN) {
        return MAX_MSG_LEN;
    }
    return val;
}

static void send_reply(void *socket, const char *reply) {
    size_t len = (reply != NULL) ? strlen(reply) : 0;
    zmq_send(socket, reply != NULL ? reply : "", len + 1, 0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <worker port 1> [<worker port 2> ...]\n", argv[0]);
        return 1;
    }

    int n_ports = argc - 1;
    printf("Starting worker on %d port(s)...\n", n_ports);

    void *context = zmq_ctx_new();
    if (context == NULL) {
        fprintf(stderr, "Failed to create ZMQ context\n");
        return 1;
    }

    void **reps = calloc((size_t)n_ports, sizeof(void *));
    if (reps == NULL) {
        fprintf(stderr, "Out of memory\n");
        zmq_ctx_destroy(context);
        return 1;
    }

    for (int i = 0; i < n_ports; ++i) {
        reps[i] = zmq_socket(context, ZMQ_REP);
        if (reps[i] == NULL) {
            fprintf(stderr, "Failed to create REP socket for port %s\n", argv[i + 1]);
            reps[i] = NULL;
            continue;
        }
        char endpoint[128];
        snprintf(endpoint, sizeof(endpoint), "tcp://*:%s", argv[i + 1]);
        if (zmq_bind(reps[i], endpoint) != 0) {
            fprintf(stderr, "Failed to bind to %s: %s\n", endpoint, zmq_strerror(zmq_errno()));
            zmq_close(reps[i]);
            reps[i] = NULL;
            continue;
        }
        printf("Bound REP socket to %s\n", endpoint);
    }

    signal(SIGINT, handle_sigint);

    zmq_pollitem_t *items = calloc((size_t)n_ports, sizeof(zmq_pollitem_t));
    if (items == NULL) {
        fprintf(stderr, "Out of memory (poll items)\n");
        for (int i = 0; i < n_ports; ++i) {
            if (reps[i]) {
                zmq_close(reps[i]);
            }
        }
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
        if (reps[i]) {
            active++;
        }
    }

    while (running && active > 0) {
        int rc = zmq_poll(items, n_ports, 1000);
        if (rc == -1) {
            if (errno == EINTR) {
                break;
            }
            perror("zmq_poll");
            break;
        }

        for (int i = 0; i < n_ports; ++i) {
            if (!reps[i]) {
                continue;
            }
            if (items[i].revents & ZMQ_POLLIN) {
                char buffer[RECV_BUFFER_SIZE];
                int r = zmq_recv(reps[i], buffer, sizeof(buffer), 0);
                if (r < 0) {
                    fprintf(stderr, "recv error on port %s: %s\n", argv[i + 1], zmq_strerror(zmq_errno()));
                    continue;
                }

                if (r >= (int)sizeof(buffer)) {
                    send_reply(reps[i], "");
                    continue;
                }
                buffer[r] = '\0';

                if (r < HEADER_LEN) {
                    send_reply(reps[i], "");
                    continue;
                }

                const char *type = buffer;
                size_t msg_len = parse_msg_len(buffer + 3);
                const char *payload = buffer + HEADER_LEN;

                if (strncmp(type, "map", 3) == 0) {
                    HashMap *hm = hm_create(1024);
                    if (hm == NULL) {
                        send_reply(reps[i], "");
                        continue;
                    }
                    wc_tokenize_and_count(hm, payload);
                    char *reply = wc_to_map_format(hm, msg_len);
                    if (reply == NULL) {
                        send_reply(reps[i], "");
                    } else {
                        send_reply(reps[i], reply);
                        free(reply);
                    }
                    hm_free(hm);
                } else if (strncmp(type, "red", 3) == 0) {
                    HashMap *hm = wc_from_map_format(payload);
                    if (hm == NULL) {
                        send_reply(reps[i], "");
                        continue;
                    }
                    char *reply = wc_to_reduce_format(hm, msg_len);
                    if (reply == NULL) {
                        send_reply(reps[i], "");
                    } else {
                        send_reply(reps[i], reply);
                        free(reply);
                    }
                    hm_free(hm);
                } else if (strncmp(type, "rip", 3) == 0) {
                    send_reply(reps[i], "rip");
                    zmq_close(reps[i]);
                    reps[i] = NULL;
                    items[i].socket = NULL;
                    active--;
                    printf("Closed worker on port %s\n", argv[i + 1]);
                } else {
                    send_reply(reps[i], "");
                }
            }
        }
    }

    free(items);
    for (int i = 0; i < n_ports; ++i) {
        if (reps[i]) {
            zmq_close(reps[i]);
        }
    }
    free(reps);
    zmq_ctx_destroy(context);
    return 0;
}
