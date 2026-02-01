#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

int run_distributor(
    const char *filename,
    int worker_count,
    char **worker_ports
);

#endif