FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-11 \
    g++-11 \
    cmake \
    make \
    python3 \
    python3-pip \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# gcc/g++ sollen auf gcc-11/g++-11 zeigen (wie auf vielen Systemen)
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 \
 && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100

# pytest exakt wie bei dir
RUN python3 -m pip install --no-cache-dir pytest==6.2.5

WORKDIR /workspace
CMD ["/bin/bash"]
