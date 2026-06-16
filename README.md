# Chord DHT Server & Distributed Systems in C

Low-level C projects in computer networking and distributed systems: a from-scratch
HTTP/1.1 server over BSD sockets, extended into a node of a **Chord Distributed Hash
Table**, plus a MapReduce-style distributed word count built on ZeroMQ message passing.

[![CI](https://github.com/AmkoAmin/chord-dht-server/actions/workflows/build.yml/badge.svg)](https://github.com/AmkoAmin/chord-dht-server/actions/workflows/build.yml)

## Stack

C11 · BSD Sockets · HTTP/1.1 · Chord DHT · ZeroMQ · CMake · Docker

## Projects

| Project | What it does | Key concepts |
|---|---|---|
| `tcp-http-server` | HTTP/1.1 server over TCP, written from scratch (`getaddrinfo`, `socket`, `bind`, `listen`, `accept`); parses request lines, headers and body, returns proper status codes | TCP sockets, HTTP request parsing, status codes |
| `chord-node` | Extends the server into a **Chord DHT** node; keys are distributed across the ring, requests for foreign keys are forwarded via UDP lookups to successor/predecessor, serving TCP (HTTP) and UDP (DHT) concurrently with `poll()` | Distributed hash tables, Chord ring, UDP lookups, event loop |
| `mapreduce-wordcount` | **MapReduce-style word count**: a distributor splits input into chunks at safe word boundaries and dispatches them over ZeroMQ to multiple workers; each worker counts words in its own hashmap and results are merged | Message passing (ZeroMQ), data partitioning, hashmap, MapReduce |

## Run with Docker

A multi-stage `Dockerfile` builds all three projects, and `docker-compose.yml`
exposes one runnable demo per profile, with no local toolchain needed:

```bash
# 1) MapReduce word count (one-shot, prints merged counts and exits)
docker compose --profile wordcount up --build

# 2) HTTP/1.1 server on http://localhost:8080
docker compose --profile http up --build

# 3) Chord DHT: a 3-node ring with static IPs
docker compose --profile chord up --build
```

The Chord ring demonstrates real DHT routing. A key a node does not own triggers
a UDP lookup to its successor (`503` while resolving), then a redirect to the
responsible node:

```bash
curl -i http://localhost:8080/dynamic/beta
# 1st request:  HTTP/1.1 503 Service Unavailable   (lookup in progress)
# retry:        HTTP/1.1 303 See Other
#               Location: http://172.28.0.13:8080/dynamic/beta
```

## Build without Docker

Each project is self-contained and built with CMake:

```bash
cd <project>
cmake -B build && make -C build
```

**Dependencies:** GCC/CMake (all projects) and `libzmq3-dev` for `mapreduce-wordcount`.

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake libzmq3-dev
```

Run the binaries directly:

```bash
# tcp-http-server / chord-node: start the web server on port 8080
./tcp-http-server/build/webserver 0.0.0.0 8080

# mapreduce-wordcount: start the worker, then the distributor
./mapreduce-wordcount/build/zmq_worker <port_1> [<port_2> ...]
./mapreduce-wordcount/build/zmq_distributor <input-file> <port_1> [<port_2> ...]
```

## Tests

The `test/` folders contain a **provided test skeleton**, not my own code. The tests
are tuned to a fixed grading environment (Ubuntu 20, Python 3.8, GCC 9) and are not
reliable elsewhere. The CI in this repo therefore verifies that all projects **compile
cleanly**.

---

Implementation: Amin Skenderi.
