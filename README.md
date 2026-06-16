# Rechnernetze und Verteilte Systeme

Systemnahe C-Implementierungen aus dem Modul **Rechnernetze und Verteilte Systeme**
(EECS, TU Berlin). Die Projekte gehen von einem einfachen TCP-Socket bis zu einem
verteilten Hash-Table-Ring und einem MapReduce-artigen WordCount über ZeroMQ.

Schwerpunkte: **Netzwerkprogrammierung mit BSD-Sockets, das HTTP/1.1-Protokoll,
verteilte Systeme (Chord-DHT) und Message-Passing-Parallelität.** Alles in C11,
gebaut mit CMake.

[![CI](https://github.com/AmkoAmin/Rechnernetze-und-Verteilte-Systeme/actions/workflows/build.yml/badge.svg)](https://github.com/AmkoAmin/Rechnernetze-und-Verteilte-Systeme/actions/workflows/build.yml)

---

## Projekte

### praxis0 — Toolchain & Setup
Minimales „Hello World" zur Verifikation der CMake-/GCC-Toolchain.

### praxis1 — HTTP-Server über TCP
Ein von Grund auf in C geschriebener Webserver auf Basis von BSD-Sockets
(`getaddrinfo`, `socket`, `bind`, `listen`, `accept`). Verarbeitet HTTP/1.1-Requests
und liefert passende Statuscodes. Eigener Message-Handler zum Parsen von
Request-Zeilen, Headern und Body.

- **Kern:** `webserver.c`, `message_handler.c`
- **Konzepte:** TCP-Verbindungen, HTTP-Request-Parsing, Statuscodes

### praxis2 — HTTP-Server + Chord-DHT
Erweiterung des Webservers zu einem Knoten in einem **Chord Distributed Hash Table**.
Schlüssel werden über den Ring aus Knoten verteilt; Anfragen für fremde Schlüssel
werden per UDP-Lookup an Successor/Predecessor weitergeleitet. Nutzt `poll()` für
gleichzeitiges Bedienen von TCP- (HTTP) und UDP- (DHT) Sockets.

- **Kern:** `webserver.c`, `http.c`, `data.c`, `util.c`
- **Konzepte:** Distributed Hash Tables, Chord-Ring, UDP-Lookups, Event-Loop mit `poll()`

### praxis3 — Verteiltes WordCount über ZeroMQ
Ein MapReduce-artiger Aufbau: Ein **Distributor** zerteilt den Eingabetext in Chunks
(an sicheren Wortgrenzen) und verteilt sie über ZeroMQ an mehrere **Worker**. Jeder
Worker zählt Wörter in einer eigenen Hashmap; der Distributor führt die Teilergebnisse
zusammen.

- **Kern:** `zmq_distributor.c`, `zmq_worker.c`, `chunker.c`, `combine.c`, `hashmap.c`, `linked_list.c`
- **Konzepte:** Message-Passing (ZeroMQ), Datenpartitionierung, Hashmap, MapReduce-Pattern
- **Testdaten:** Public-Domain-Texte aus dem Project Gutenberg (`test_files/`)

---

## Bauen

Jedes Projekt ist eigenständig und wird mit CMake gebaut:

```bash
cd praxisX
cmake -B build && make -C build
```

**Abhängigkeiten:** GCC/CMake (alle Projekte) und `libzmq3-dev` für praxis3.

```bash
# Debian/Ubuntu
sudo apt-get install build-essential cmake libzmq3-dev
```

### Reproduzierbar per Docker

Das mitgelieferte `Dockerfile` bildet die Build-Umgebung nach:

```bash
docker build -t rnv . && docker run --rm -it -v "$PWD:/workspace" rnv
```

---

## Beispiele

```bash
# praxis1/2: Webserver auf Port 8080 starten
./praxis1/build/webserver 0.0.0.0 8080

# praxis3: Worker und Distributor starten
./praxis3/build/zmq_worker <distributor-host> <port>
./praxis3/build/zmq_distributor <input-file> <worker-endpoints...>
```

---

## Hinweis zu den Tests

Die `test/`-Ordner enthalten das **vom Kurs vorgegebene Test-Skeleton** (TU Berlin / TKN),
nicht meinen eigenen Code. Die Tests sind auf die EECS-Prüfumgebung abgestimmt
(Ubuntu 20, Python 3.8, GCC 9) und laufen außerhalb davon nicht zuverlässig.
Die CI in diesem Repo prüft daher, dass alle Projekte **fehlerfrei kompilieren**.

---

© Aufgabenstellung: Technische Universität Berlin · Fachgebiet Telekommunikationsnetze (TKN).
Implementierung: Amin Skenderi.
