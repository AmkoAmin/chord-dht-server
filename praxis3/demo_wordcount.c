#include "hashmap.h"
#include <stdio.h>

/*
 * Demonstration of hashmap usage in distributed wordcount (map/reduce pattern)
 */

void print_entry(const char *key, int count, void *ctx) {
    (void)ctx;
    printf("  %-15s: %d\n", key, count);
}

int main(void) {
    printf("=== Distributed Word Count Simulation ===\n\n");

    /* Simulating map phase: split text into chunks and count words */
    printf("MAP PHASE - Node 1 processes chunk 1:\n");
    HashMap *node1_map = hm_create(128);
    hm_put_inc(node1_map, "the", 3);
    hm_put_inc(node1_map, "quick", 1);
    hm_put_inc(node1_map, "brown", 1);
    hm_put_inc(node1_map, "fox", 1);
    hm_put_inc(node1_map, "jumps", 1);
    printf("  Entries: %zu\n", hm_entry_count(node1_map));

    printf("\nMAP PHASE - Node 2 processes chunk 2:\n");
    HashMap *node2_map = hm_create(128);
    hm_put_inc(node2_map, "The", 2);       /* case-insensitive */
    hm_put_inc(node2_map, "QUICK", 1);
    hm_put_inc(node2_map, "brown", 2);
    hm_put_inc(node2_map, "FOX", 2);
    hm_put_inc(node2_map, "over", 1);
    printf("  Entries: %zu\n", hm_entry_count(node2_map));

    printf("\nCOMBINE PHASE - Merge results:\n");
    HashMap *global_map = hm_create(256);
    printf("  Merging node1_map...\n");
    hm_merge(global_map, node1_map);
    printf("  Merging node2_map...\n");
    hm_merge(global_map, node2_map);
    printf("  Total unique words: %zu\n", hm_entry_count(global_map));

    printf("\nREDUCE PHASE - Final word counts (sorted by case-insensitive key):\n");
    hm_foreach(global_map, print_entry, NULL);

    printf("\nVERIFICATION:\n");
    int count;
    hm_get(global_map, "the", &count);
    printf("  'the' (normalized from 'The'/'THE'): %d (expected 5)\n", count);
    hm_get(global_map, "brown", &count);
    printf("  'brown': %d (expected 3)\n", count);
    hm_get(global_map, "fox", &count);
    printf("  'fox' (normalized from 'FOX'): %d (expected 3)\n", count);

    printf("\nCLEANUP:\n");
    hm_free(node1_map);
    hm_free(node2_map);
    hm_free(global_map);
    printf("  All hashmaps freed\n");

    return 0;
}
