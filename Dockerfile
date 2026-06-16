# syntax=docker/dockerfile:1

# ---------- build stage: compile all three projects ----------
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libzmq3-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Out-of-source build for each self-contained project.
RUN for p in tcp-http-server chord-node mapreduce-wordcount; do \
        cmake -B "$p/build" "$p" && make -C "$p/build"; \
    done

# ---------- runtime stage: slim image with the built binaries ----------
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        libzmq5 \
        curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /src /app

# Default: drop into a shell with all binaries pre-built under <project>/build/.
CMD ["/bin/bash"]
