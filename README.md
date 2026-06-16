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
| `praxis0` | Toolchain sanity check ("Hello World") | CMake / GCC setup |
| `praxis1` | HTTP/1.1 server over TCP, written from scratch (`getaddrinfo`, `socket`, `bind`, `listen`, `accept`); parses request lines, headers and body, returns proper status codes | TCP sockets, HTTP request parsing, status codes |
| `praxis2` | Extends the server into a **Chord DHT** node; keys are distributed across the ring, requests for foreign keys are forwarded via UDP lookups to successor/predecessor, serving TCP (HTTP) and UDP (DHT) concurrently with `poll()` | Distributed hash tables, Chord ring, UDP lookups, event loop |
| `praxis3` | **MapReduce-style word count**: a distributor splits input into chunks at safe word boundaries and dispatches them over ZeroMQ to multiple workers; each worker counts words in its own hashmap and results are merged | Message passing (ZeroMQ), data partitioning, hashmap, MapReduce |

## Build

Each project is self-contained and built with CMake:

```bash
cd praxisX
cmake -B build && make -C build
```

**Dependencies:** GCC/CMake (all projects) and `libzmq3-dev` for `praxis3`.

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake libzmq3-dev
```

### Reproducible build with Docker

The bundled `Dockerfile` reproduces the build environment:

```bash
docker build -t rnv . && docker run --rm -it -v "$PWD:/workspace" rnv
```

## Usage

```bash
# praxis1 / praxis2: start the web server on port 8080
./praxis1/build/webserver 0.0.0.0 8080

# praxis3: start workers, then the distributor
./praxis3/build/zmq_worker <distributor-host> <port>
./praxis3/build/zmq_distributor <input-file> <worker-endpoints...>
```

## Tests

The `test/` folders contain a **provided test skeleton**, not my own code. The tests
are tuned to a fixed grading environment (Ubuntu 20, Python 3.8, GCC 9) and are not
reliable elsewhere. The CI in this repo therefore verifies that all projects **compile
cleanly**.

---

Implementation: Amin Skenderi.
