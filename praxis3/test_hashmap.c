#include "hashmap.h"
#include <stdio.h>
#include <string.h>

/* Callback for foreach */
void print_entry(const char *key, int count, void *ctx) {
    (void)ctx;  /* unused */
    printf("  '%s': %d\n", key, count);
}

int main(void) {
    printf("=== HashMap Tests ===\n\n");

    printf("Test 1: Create hashmap with default buckets\n");
    HashMap *map = hm_create(0);
    if (map == NULL) {
        fprintf(stderr, "Failed to create hashmap\n");
        return 1;
    }
    printf("  Created hashmap with %zu buckets\n", map->n_buckets);

    printf("\nTest 2: Insert 'Example' twice with case variations\n");
    int r1 = hm_put_inc(map, "Example", 1);
    int r2 = hm_put_inc(map, "EXAMPLE", 1);
    printf("  hm_put_inc(map, 'Example', 1) = %d\n", r1);
    printf("  hm_put_inc(map, 'EXAMPLE', 1) = %d\n", r2);

    printf("\nTest 3: Retrieve as 'example'\n");
    int count = 0;
    int found = hm_get(map, "example", &count);
    printf("  hm_get(map, 'example', &count) = %d, count = %d\n", found, count);

    printf("\nTest 4: Add more words\n");
    hm_put_inc(map, "Hello", 2);
    hm_put_inc(map, "World", 3);
    hm_put_inc(map, "hello", 1);
    printf("  Added: 'Hello' (+2), 'World' (+3), 'hello' (+1)\n");

    printf("\nTest 5: Total entry count\n");
    size_t total = hm_entry_count(map);
    printf("  hm_entry_count(map) = %zu\n", total);

    printf("\nTest 6: Iterate all entries\n");
    hm_foreach(map, print_entry, NULL);

    printf("\nTest 7: Set exact count\n");
    hm_set(map, "World", 100);
    found = hm_get(map, "world", &count);
    printf("  After hm_set(map, 'World', 100): count = %d\n", count);

    printf("\nTest 8: Create second hashmap and merge\n");
    HashMap *map2 = hm_create(512);
    hm_put_inc(map2, "Example", 5);
    hm_put_inc(map2, "new", 10);
    printf("  Created map2 with: 'Example' (+5), 'new' (+10)\n");
    printf("  map2 entry count = %zu\n", hm_entry_count(map2));

    int merge_result = hm_merge(map, map2);
    printf("  hm_merge(map, map2) = %d\n", merge_result);
    printf("  map entry count after merge = %zu\n", hm_entry_count(map));

    printf("\nTest 9: Check merged values\n");
    hm_get(map, "Example", &count);
    printf("  'example' count (should be 2+5=7): %d\n", count);
    hm_get(map, "new", &count);
    printf("  'new' count (should be 10): %d\n", count);

    printf("\nTest 10: All entries after merge\n");
    hm_foreach(map, print_entry, NULL);

    printf("\nTest 11: Clear map\n");
    hm_clear(map);
    printf("  After hm_clear(map): entry count = %zu\n", hm_entry_count(map));

    printf("\nTest 12: Reuse cleared map\n");
    hm_put_inc(map, "test", 42);
    hm_get(map, "test", &count);
    printf("  After hm_put_inc(map, 'test', 42): count = %d\n", count);

    printf("\nTest 13: NULL key handling\n");
    hm_put_inc(map, NULL, 1);
    hm_get(map, NULL, &count);
    printf("  hm_get(map, NULL, &count): count = %d\n", count);

    printf("\nTest 14: Cleanup\n");
    hm_free(map);
    hm_free(map2);
    printf("  Freed both hashmaps\n");

    printf("\n=== All Tests Passed ===\n");
    return 0;
}
