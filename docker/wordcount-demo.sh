#!/usr/bin/env bash
# Runs the MapReduce-style word count end to end inside a single container:
# starts a worker bound to two ports, then runs the distributor against an
# input file. The distributor prints the merged word counts and sends a
# shutdown ("rip") message, after which the worker exits.
set -euo pipefail

cd /app/mapreduce-wordcount

PORTS=(5555 5556)
INPUT="${INPUT:-/app/docker/sample.txt}"

echo "[demo] starting worker on ports ${PORTS[*]} ..."
./build/zmq_worker "${PORTS[@]}" &
WORKER_PID=$!

# give the worker a moment to bind its sockets
sleep 1

echo "[demo] running distributor on ${INPUT} ..."
echo "----------------------------------------"
./build/zmq_distributor "${INPUT}" "${PORTS[@]}"
echo "----------------------------------------"

wait "${WORKER_PID}" 2>/dev/null || true
echo "[demo] done."
