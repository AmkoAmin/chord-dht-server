#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "data.h"
#include "http.h"
#include "util.h"

#define MAX_RESOURCES 100

// Chord peer information
struct chord_peer {
    uint16_t id;
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
};

// Global Chord state (set in main())
static uint16_t self_id = 0;
static struct chord_peer successor = {0};
static struct chord_peer predecessor = {0};
// Store self network identity for UDP lookup replies
static char self_ip[INET_ADDRSTRLEN] = {0};
static uint16_t self_port = 0;
// Simple reply cache for most-recent reply (sufficient for tests)
static bool have_reply = false;
static struct chord_peer reply_peer = {0};
static uint16_t reply_prev_id = 0; // "Hash ID" field from reply (treated as predecessor id)

struct tuple resources[MAX_RESOURCES] = {
    {"/static/foo", "Foo", sizeof "Foo" - 1},
    {"/static/bar", "Bar", sizeof "Bar" - 1},
    {"/static/baz", "Baz", sizeof "Baz" - 1}};

/**
 * Sends an HTTP reply to the client based on the received request.
 *
 * @param conn      The file descriptor of the client connection socket.
 * @param request   A pointer to the struct containing the parsed request
 * information.
 */
void send_reply(int conn, struct request *request) {

    // Create a buffer to hold the HTTP reply
    char buffer[HTTP_MAX_SIZE];
    char *reply = buffer;
    size_t offset = 0;

    fprintf(stderr, "Handling %s request for %s (%lu byte payload)\n",
            request->method, request->uri, request->payload_length);

    if (strcmp(request->method, "GET") == 0) {
        // Find the resource with the given URI in the 'resources' array.
        size_t resource_length;
        const char *resource =
            get(request->uri, resources, MAX_RESOURCES, &resource_length);

        if (resource) {
            size_t payload_offset =
                sprintf(reply, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n\r\n",
                        resource_length);
            memcpy(reply + payload_offset, resource, resource_length);
            offset = payload_offset + resource_length;
        } else {
            reply = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            offset = strlen(reply);
        }
    } else if (strcmp(request->method, "PUT") == 0) {
        // Try to set the requested resource with the given payload in the
        // 'resources' array.
        if (set(request->uri, request->payload, request->payload_length,
                resources, MAX_RESOURCES)) {
            reply = "HTTP/1.1 204 No Content\r\n\r\n";
        } else {
            reply = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
        }
        offset = strlen(reply);
    } else if (strcmp(request->method, "DELETE") == 0) {
        // Try to delete the requested resource from the 'resources' array
        if (delete (request->uri, resources, MAX_RESOURCES)) {
            reply = "HTTP/1.1 204 No Content\r\n\r\n";
        } else {
            reply = "HTTP/1.1 404 Not Found\r\n\r\n";
        }
        offset = strlen(reply);
    } else {
        reply = "HTTP/1.1 501 Method Not Supported\r\n\r\n";
        offset = strlen(reply);
    }

    // Send the reply back to the client
    if (send(conn, reply, offset, 0) == -1) {
        perror("send");
        close(conn);
    }
}

/**
 * Checks if this node is responsible for the given hash value in the Chord ring.
 * A node is responsible if: hash > predecessor_id AND hash <= self_id
 *
 * @param hash The hash value to check
 * @param self_id The ID of this node
 * @param pred_id The ID of the predecessor node
 *
 * @return true if this node is responsible, false otherwise
 */
static bool is_responsible(uint16_t hash, uint16_t self_id, uint16_t pred_id) {
    /* Handle wrap-around in the identifier space.
     * Normal case (no wrap): pred_id < self_id
     *   responsible if (pred_id, self_id]
     * Wrap case (pred_id >= self_id): interval wraps around 0
     *   responsible if (pred_id, 0xFFFF] U [0x0000, self_id]
     */
    if (pred_id < self_id) {
        return (hash > pred_id && hash <= self_id);
    } else {
        return (hash > pred_id || hash <= self_id);
    }
}

/**
 * Sends an HTTP 303 redirect response to redirect the request to the successor.
 *
 * @param conn The socket descriptor of the client connection
 * @param uri The resource URI to redirect to
 */
static void send_redirect(int conn, const char *uri) {
    char reply[HTTP_MAX_SIZE];
    size_t offset = 0;

    // Build Location header with successor's address
    offset = snprintf(reply, sizeof(reply),
                     "HTTP/1.1 303 See Other\r\n"
                     "Location: http://%s:%u%s\r\n"
                     "Content-Length: 0\r\n"
                     "\r\n",
                     successor.ip, successor.port, uri);

    if (offset >= sizeof(reply)) {
        fprintf(stderr, "Redirect URL too long, truncated\n");
        offset = sizeof(reply) - 1;
    }

    if (send(conn, reply, offset, 0) == -1) {
        perror("send redirect");
    }

    fprintf(stderr, "Sent redirect to %s:%u%s\n", successor.ip, successor.port, uri);
}

/**
 * Processes an incoming packet from the client.
 *
 * @param conn The socket descriptor representing the connection to the client.
 * @param buffer A pointer to the incoming packet's buffer.
 * @param n The size of the incoming packet.
 *
 * @return Returns the number of bytes processed from the packet.
 *         If the packet is successfully processed and a reply is sent, the
 * return value indicates the number of bytes processed. If the packet is
 * malformed or an error occurs during processing, the return value is -1.
 *
 */
ssize_t process_packet(int conn, char *buffer, size_t n) {
    struct request request = {
        .method = NULL, .uri = NULL, .payload = NULL, .payload_length = -1};
    ssize_t bytes_processed = parse_request(buffer, n, &request);

    // Hash the URI to determine which peer owns this resource
    uint16_t uri_hash = 0;
    if (bytes_processed > 0 && request.uri) {
        uri_hash = pseudo_hash((const unsigned char *)request.uri, strlen(request.uri));
    }

    if (bytes_processed > 0) {
        // Determine ring size cases
        bool single_node = false;
        if ((predecessor.id == self_id && successor.id == self_id) ||
            (predecessor.id == 0 && successor.id == 0)) {
            single_node = true;
        }

        bool responsible = false;
        if (single_node) {
            // Single-node ring: always responsible for all keys
            responsible = true;
        } else {
            responsible = is_responsible(uri_hash, self_id, predecessor.id);
        }

        if (!responsible) {
            // Not responsible: decide behavior depending on ring size
            if (successor.id == predecessor.id) {
                // Two-node ring: redirect to successor (no UDP lookup)
                fprintf(stderr, "Two-node ring, not responsible for hash %u (self=%u, pred=%u), redirecting to successor\n",
                        uri_hash, self_id, predecessor.id);
                send_redirect(conn, request.uri);
            } else {
                // Ring with >2 nodes: first check cache for a stored reply
                if (have_reply) {
                    // Use cached reply to redirect client
                    fprintf(stderr, "Using cached reply to redirect to peer %s:%u\n", reply_peer.ip, reply_peer.port);
                    // Build redirect to cached peer
                    char rep[HTTP_MAX_SIZE];
                    size_t off = snprintf(rep, sizeof(rep),
                                          "HTTP/1.1 303 See Other\r\n"
                                          "Location: http://%s:%u%s\r\n"
                                          "Content-Length: 0\r\n"
                                          "\r\n",
                                          reply_peer.ip, reply_peer.port, request.uri);
                    if (off >= sizeof(rep)) off = sizeof(rep) - 1;
                    send(conn, rep, off, 0);
                    // clear cache entry after use
                    have_reply = false;
                } else {
                    // If this node knows that its successor is responsible for the hash,
                    // synthesize a local REPLY (store in cache) so originator immediately
                    // has the information available (special case: send to self)
                    if (is_responsible(uri_hash, successor.id, self_id)) {
                        // Successor is responsible; store reply locally: id = this node's id
                        have_reply = true;
                        reply_prev_id = self_id; // predecessor of responsible is this node
                        reply_peer = successor;
                        fprintf(stderr, "Locally stored REPLY (successor responsible): peer=%s:%u prev_id=%u\n",
                                reply_peer.ip, reply_peer.port, reply_prev_id);
                        // Send 503 to HTTP client (as before)
                        char rep[HTTP_MAX_SIZE];
                        size_t off = snprintf(rep, sizeof(rep),
                                              "HTTP/1.1 503 Service Unavailable\r\n"
                                              "Retry-After: 1\r\n"
                                              "Content-Length: 0\r\n"
                                              "\r\n");
                        if (off >= sizeof(rep)) off = sizeof(rep) - 1;
                        if (send(conn, rep, off, 0) == -1) perror("send 503");
                    } else {
                        // No cached reply and successor not known to be responsible: send 503 and UDP lookup to successor
                        fprintf(stderr, ">2-node ring, not responsible for hash %u, sending 503 and UDP lookup\n",
                                uri_hash);
                        // Send 503 Service Unavailable with Retry-After: 1
                        char rep[HTTP_MAX_SIZE];
                        size_t off = snprintf(rep, sizeof(rep),
                                              "HTTP/1.1 503 Service Unavailable\r\n"
                                              "Retry-After: 1\r\n"
                                              "Content-Length: 0\r\n"
                                              "\r\n");
                        if (off >= sizeof(rep)) off = sizeof(rep) - 1;
                        if (send(conn, rep, off, 0) == -1) {
                            perror("send 503");
                        }

                        // Send UDP lookup to successor
                        int sock = socket(AF_INET, SOCK_DGRAM, 0);
                        if (sock == -1) {
                            perror("socket udp lookup");
                        } else {
                            struct sockaddr_in succ_addr = {0};
                            succ_addr.sin_family = AF_INET;
                            succ_addr.sin_port = htons(successor.port);
                            if (inet_pton(AF_INET, successor.ip, &succ_addr.sin_addr) != 1) {
                                fprintf(stderr, "Invalid successor IP %s\n", successor.ip);
                            } else {
                                unsigned char msg[11];
                                // flags = LOOKUP (0)
                                msg[0] = 0;
                                // id (hash)
                                uint16_t net_id = htons(uri_hash);
                                memcpy(&msg[1], &net_id, 2);
                                // peer.id = self_id
                                uint16_t net_peer_id = htons(self_id);
                                memcpy(&msg[3], &net_peer_id, 2);
                                // peer.ip = self_ip
                                unsigned char ipbuf[4] = {0};
                                if (inet_pton(AF_INET, self_ip, ipbuf) != 1) {
                                    fprintf(stderr, "Invalid self IP %s\n", self_ip);
                                }
                                memcpy(&msg[5], ipbuf, 4);
                                // peer.port
                                uint16_t net_peer_port = htons(self_port);
                                memcpy(&msg[9], &net_peer_port, 2);

                                ssize_t sent = sendto(sock, msg, sizeof(msg), 0,
                                                      (struct sockaddr *)&succ_addr, sizeof(succ_addr));
                                if (sent == -1) perror("sendto lookup");
                            }
                            close(sock);
                        }
                    }
                }
            }
        } else {
            // This node is responsible, process the request
            send_reply(conn, &request);
        }

        // Check the "Connection" header in the request to determine if the
        // connection should be kept alive or closed.
        const string connection_header = get_header(&request, "Connection");
        if (connection_header && strcmp(connection_header, "close")) {
            return -1;
        }
    } else if (bytes_processed == -1) {
        // If the request is malformed or an error occurs during processing,
        // send a 400 Bad Request response to the client.
        const string bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(conn, bad_request, strlen(bad_request), 0);
        printf("Received malformed request, terminating connection.\n");
        close(conn);
        return -1;
    }

    return bytes_processed;
}

/**
 * Sets up the connection state for a new socket connection.
 *
 * @param state A pointer to the connection_state structure to be initialized.
 * @param sock The socket descriptor representing the new connection.
 *
 */
static void connection_setup(struct connection_state *state, int sock) {
    // Set the socket descriptor for the new connection in the connection_state
    // structure.
    state->sock = sock;

    // Set the 'end' pointer of the state to the beginning of the buffer.
    state->end = state->buffer;

    // Clear the buffer by filling it with zeros to avoid any stale data.
    memset(state->buffer, 0, HTTP_MAX_SIZE);
}

/**
 * Discards the front of a buffer
 *
 * @param buffer A pointer to the buffer to be modified.
 * @param discard The number of bytes to drop from the front of the buffer.
 * @param keep The number of bytes that should be kept after the discarded
 * bytes.
 *
 * @return Returns a pointer to the first unused byte in the buffer after the
 * discard.
 * @example buffer_discard(ABCDEF0000, 4, 2):
 *          ABCDEF0000 ->  EFCDEF0000 -> EF00000000, returns pointer to first 0.
 */
char *buffer_discard(char *buffer, size_t discard, size_t keep) {
    memmove(buffer, buffer + discard, keep);
    memset(buffer + keep, 0, discard); // invalidate buffer
    return buffer + keep;
}

/**
 * Handles incoming UDP datagrams in a non-blocking loop.
 * Reads all pending UDP packets, logs them with debug info, and handles errors gracefully.
 *
 * @param udp_sock The UDP socket file descriptor (must be non-blocking).
 */
static void handle_udp_socket(int udp_sock) {
    // Buffer size for UDP datagram (HTTP_MAX_SIZE is a reasonable upper bound)
    char buffer[HTTP_MAX_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    while (1) {
        // Receive datagram from any sender
        ssize_t bytes = recvfrom(udp_sock, buffer, sizeof(buffer) - 1, 0,
                                 (struct sockaddr *)&sender, &sender_len);

        if (bytes == -1) {
            // Non-blocking socket returns EAGAIN/EWOULDBLOCK when no data
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Normal case: no more data to read
                break;
            } else {
                // Unexpected error, but don't kill the server
                perror("recvfrom");
                break;
            }
        } else if (bytes == 0) {
            // Shouldn't happen with UDP (no graceful close), but handle it
            break;
        } else {
            // Successfully received a datagram
            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender.sin_addr, sender_ip, sizeof(sender_ip));
            uint16_t sender_port = ntohs(sender.sin_port);

            fprintf(stderr, "[UDP] Received %zd bytes from %s:%u\n",
                    bytes, sender_ip, sender_port);

            // We expect DHT messages of 11 bytes: flags(1), id(2), peer.id(2), peer.ip(4), peer.port(2)
            if (bytes < 11) {
                fprintf(stderr, "[UDP] Ignoring short packet (%zd bytes)\n", bytes);
                continue;
            }

            unsigned char *msg = (unsigned char *)buffer;
            uint8_t flags = msg[0];
            uint16_t mid = ntohs(*(uint16_t *)&msg[1]);
            uint16_t peer_id = ntohs(*(uint16_t *)&msg[3]);
            char peer_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &msg[5], peer_ip, sizeof(peer_ip));
            uint16_t peer_port = ntohs(*(uint16_t *)&msg[9]);

            if (flags == 0) {
                // LOOKUP: msg.id is the queried hash, peer is originator
                uint16_t query_hash = mid;
                struct chord_peer origin = {peer_id, "", peer_port};
                strncpy(origin.ip, peer_ip, INET_ADDRSTRLEN - 1);

                // If this node is responsible for the hash -> reply with responsible=self
                if (is_responsible(query_hash, self_id, predecessor.id)) {
                    // Build reply: flags=1, id = predecessor of responsible node (predecessor.id), peer = responsible node (self)
                    unsigned char out[11];
                    out[0] = 1; // reply
                    uint16_t net_id = htons(predecessor.id);
                    memcpy(&out[1], &net_id, 2);
                    uint16_t net_peer_id = htons(self_id);
                    memcpy(&out[3], &net_peer_id, 2);
                    unsigned char ipbuf[4];
                    if (inet_pton(AF_INET, self_ip, ipbuf) != 1) memset(ipbuf,0,4);
                    memcpy(&out[5], ipbuf, 4);
                    uint16_t net_peer_port = htons(self_port);
                    memcpy(&out[9], &net_peer_port, 2);

                    struct sockaddr_in dest = {0};
                    dest.sin_family = AF_INET;
                    dest.sin_port = htons(origin.port);
                    if (inet_pton(AF_INET, origin.ip, &dest.sin_addr) != 1) {
                        fprintf(stderr, "[UDP] Invalid origin IP %s\n", origin.ip);
                    } else {
                        sendto(udp_sock, out, sizeof(out), 0, (struct sockaddr *)&dest, sizeof(dest));
                        fprintf(stderr, "[UDP] Sent REPLY (responsible=self) to %s:%u\n", origin.ip, origin.port);
                    }
                } else if (is_responsible(query_hash, successor.id, self_id)) {
                    // Successor is responsible: reply with peer = successor and id = this node's id (predecessor of responsible)
                    unsigned char out[11];
                    out[0] = 1; // reply
                    uint16_t net_id = htons(self_id);
                    memcpy(&out[1], &net_id, 2);
                    uint16_t net_peer_id = htons(successor.id);
                    memcpy(&out[3], &net_peer_id, 2);
                    unsigned char ipbuf[4];
                    if (inet_pton(AF_INET, successor.ip, ipbuf) != 1) memset(ipbuf,0,4);
                    memcpy(&out[5], ipbuf, 4);
                    uint16_t net_peer_port = htons(successor.port);
                    memcpy(&out[9], &net_peer_port, 2);

                    struct sockaddr_in dest = {0};
                    dest.sin_family = AF_INET;
                    dest.sin_port = htons(origin.port);
                    if (inet_pton(AF_INET, origin.ip, &dest.sin_addr) != 1) {
                        fprintf(stderr, "[UDP] Invalid origin IP %s\n", origin.ip);
                    } else {
                        sendto(udp_sock, out, sizeof(out), 0, (struct sockaddr *)&dest, sizeof(dest));
                        fprintf(stderr, "[UDP] Sent REPLY (responsible=successor) to %s:%u\n", origin.ip, origin.port);
                    }
                } else {
                    // Forward lookup to successor unchanged
                    struct sockaddr_in succ_addr = {0};
                    succ_addr.sin_family = AF_INET;
                    succ_addr.sin_port = htons(successor.port);
                    if (inet_pton(AF_INET, successor.ip, &succ_addr.sin_addr) != 1) {
                        fprintf(stderr, "[UDP] Invalid successor IP %s\n", successor.ip);
                    } else {
                        ssize_t s = sendto(udp_sock, msg, 11, 0, (struct sockaddr *)&succ_addr, sizeof(succ_addr));
                        if (s == -1) perror("sendto forward");
                        else fprintf(stderr, "[UDP] Forwarded LOOKUP to successor %s:%u\n", successor.ip, successor.port);
                    }
                }
            } else if (flags == 1) {
                // REPLY: store in cache for later HTTP handling
                have_reply = true;
                reply_prev_id = mid; // treated as "predecessor id" per protocol
                reply_peer.id = peer_id;
                strncpy(reply_peer.ip, peer_ip, INET_ADDRSTRLEN - 1);
                reply_peer.port = peer_port;
                fprintf(stderr, "[UDP] Stored REPLY: peer=%u %s:%u prev_id=%u\n",
                        reply_peer.id, reply_peer.ip, reply_peer.port, reply_prev_id);
            } else {
                fprintf(stderr, "[UDP] Unknown flags=%u\n", flags);
            }
        }
    }
}

/**
 * Handles incoming connections and processes data received over the socket.
 *
 * @param state A pointer to the connection_state structure containing the
 * connection state.
 * @return Returns true if the connection and data processing were successful,
 * false otherwise. If an error occurs while receiving data from the socket, the
 * function exits the program.
 */
bool handle_connection(struct connection_state *state) {
    // Calculate the pointer to the end of the buffer to avoid buffer overflow
    const char *buffer_end = state->buffer + HTTP_MAX_SIZE;

    // Check if an error occurred while receiving data from the socket
    ssize_t bytes_read =
        recv(state->sock, state->end, buffer_end - state->end, 0);
    if (bytes_read == -1) {
        perror("recv");
        close(state->sock);
        exit(EXIT_FAILURE);
    } else if (bytes_read == 0) {
        return false;
    }

    char *window_start = state->buffer;
    char *window_end = state->end + bytes_read;

    ssize_t bytes_processed = 0;
    while ((bytes_processed = process_packet(state->sock, window_start,
                                             window_end - window_start)) > 0) {
        window_start += bytes_processed;
    }
    if (bytes_processed == -1) {
        return false;
    }

    state->end = buffer_discard(state->buffer, window_start - state->buffer,
                                window_end - window_start);
    return true;
}

/**
 * Derives a sockaddr_in structure from the provided host and port information.
 *
 * @param host The host (IP address or hostname) to be resolved into a network
 * address.
 * @param port The port number to be converted into network byte order.
 *
 * @return A sockaddr_in structure representing the network address derived from
 * the host and port.
 */
static struct sockaddr_in derive_sockaddr(const char *host, const char *port) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
    };
    struct addrinfo *result_info;

    // Resolve the host (IP address or hostname) into a list of possible
    // addresses.
    int returncode = getaddrinfo(host, port, &hints, &result_info);
    if (returncode) {
        fprintf(stderr, "Error parsing host/port");
        exit(EXIT_FAILURE);
    }

    // Copy the sockaddr_in structure from the first address in the list
    struct sockaddr_in result = *((struct sockaddr_in *)result_info->ai_addr);

    // Free the allocated memory for the result_info
    freeaddrinfo(result_info);
    return result;
}

/**
 * Sets up a TCP server socket and binds it to the provided sockaddr_in address.
 *
 * @param addr The sockaddr_in structure representing the IP address and port of
 * the server.
 *
 * @return The file descriptor of the created TCP server socket.
 */
static int setup_server_socket(struct sockaddr_in addr) {
    const int enable = 1;
    const int backlog = 1;

    // Create a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Avoid dead lock on connections that are dropped after poll returns but
    // before accept is called
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR socket option to allow reuse of local addresses
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) ==
        -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind socket to the provided address
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Start listening on the socket with maximum backlog of 1 pending
    // connection
    if (listen(sock, backlog)) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return sock;
}

static int setup_udp_socket(struct sockaddr_in addr) {
    const int enable = 1;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket udp");
        exit(EXIT_FAILURE);
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl udp");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        perror("setsockopt udp");
        exit(EXIT_FAILURE);
    }

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind udp");
        close(sock);
        exit(EXIT_FAILURE);
    }

    return sock; // KEIN listen() bei UDP
}


/**
 *  The program expects 4 arguments: self.ip, self.port, and self.id.
 *  If self.id is empty or not provided, defaults to 0.
 *  Reads predecessor and successor information from environment variables:
 *  - PRED_ID, PRED_IP, PRED_PORT (predecessor)
 *  - SUCC_ID, SUCC_IP, SUCC_PORT (successor)
 *
 *  Call as:
 *
 *  ./build/webserver self.ip self.port [self.id]
 */
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <self.ip> <self.port> [self.id]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Parse self node ID from argv[3], default to 0 if not provided or empty
    // Store in global variable for use in process_packet()
    if (argc > 3 && argv[3] && strlen(argv[3]) > 0) {
        char *endptr;
        self_id = safe_strtoul(argv[3], &endptr, 10, "Invalid self.id");
    }
    // Store self IP/port for UDP lookup messages
    strncpy(self_ip, argv[1], INET_ADDRSTRLEN - 1);
    char *endptr;
    self_port = safe_strtoul(argv[2], &endptr, 10, "Invalid self.port");
    
    // Read predecessor information from environment variables
    const char *pred_id_str = getenv("PRED_ID");
    const char *pred_ip = getenv("PRED_IP");
    const char *pred_port_str = getenv("PRED_PORT");
    
    if (pred_id_str && pred_ip && pred_port_str) {
        char *endptr;
        predecessor.id = safe_strtoul(pred_id_str, &endptr, 10, "Invalid PRED_ID");
        strncpy(predecessor.ip, pred_ip, INET_ADDRSTRLEN - 1);
        predecessor.port = safe_strtoul(pred_port_str, &endptr, 10, "Invalid PRED_PORT");
        fprintf(stderr, "[Chord] Predecessor: id=%u, ip=%s, port=%u\n",
                predecessor.id, predecessor.ip, predecessor.port);
    }
    
    // Read successor information from environment variables
    const char *succ_id_str = getenv("SUCC_ID");
    const char *succ_ip = getenv("SUCC_IP");
    const char *succ_port_str = getenv("SUCC_PORT");
    
    if (succ_id_str && succ_ip && succ_port_str) {
        char *endptr;
        successor.id = safe_strtoul(succ_id_str, &endptr, 10, "Invalid SUCC_ID");
        strncpy(successor.ip, succ_ip, INET_ADDRSTRLEN - 1);
        successor.port = safe_strtoul(succ_port_str, &endptr, 10, "Invalid SUCC_PORT");
        fprintf(stderr, "[Chord] Successor: id=%u, ip=%s, port=%u\n",
                successor.id, successor.ip, successor.port);
    }
    
    fprintf(stderr, "[Chord] Self node ID: %u\n", self_id);
    
    struct sockaddr_in addr = derive_sockaddr(argv[1], argv[2]);

    // Set up a server socket.
    int server_socket = setup_server_socket(addr);
    int udp_socket = setup_udp_socket(addr);

    // Create an array of pollfd structures to monitor sockets:
    // [0] = server_socket (TCP accept)
    // [1] = udp_socket (UDP datagram receive) - always active
    // [2] = active TCP connection (when connected)
    struct pollfd sockets[3] = {
        {.fd = server_socket, .events = POLLIN, .revents = 0},
        {.fd = udp_socket, .events = POLLIN, .revents = 0},
        {.fd = -1, .events = 0, .revents = 0},  // TCP connection (inactive initially)
    };

    struct connection_state state = {0};
    while (true) {

        // Use poll() to wait for events on the monitored sockets.
        int ready = poll(sockets, sizeof(sockets) / sizeof(sockets[0]), -1);
        if (ready == -1) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        // Process events on the monitored sockets.
        for (size_t i = 0; i < sizeof(sockets) / sizeof(sockets[0]); i += 1) {
            if (sockets[i].revents == 0 || sockets[i].fd == -1) {
                // If there are no events on the socket or socket is inactive, continue
                continue;
            }

            int s = sockets[i].fd;

            if (s == server_socket) {
                // If the event is on the server_socket, accept a new connection
                // from a client.
                int connection = accept(server_socket, NULL, NULL);
                if (connection == -1 && errno != EAGAIN &&
                    errno != EWOULDBLOCK) {
                    close(server_socket);
                    perror("accept");
                    exit(EXIT_FAILURE);
                } else if (connection != -1) {
                    connection_setup(&state, connection);

                    // Limit to one TCP connection at a time
                    // Disable accepting new connections while one is active
                    sockets[0].events = 0;
                    // Enable the TCP connection socket in slot [2]
                    sockets[2].fd = connection;
                    sockets[2].events = POLLIN;
                }
            } else if (s == udp_socket) {
                // Handle all pending UDP datagrams
                handle_udp_socket(udp_socket);
            } else {
                // Must be the TCP connection socket
                assert(s == state.sock);

                // Call the 'handle_connection' function to process the incoming
                // data on the socket.
                bool cont = handle_connection(&state);
                if (!cont) {
                    // Connection closed, get ready for a new connection
                    close(state.sock);
                    sockets[0].events = POLLIN;   // Re-enable TCP accept
                    sockets[2].fd = -1;            // Disable TCP connection slot
                    sockets[2].events = 0;
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
