#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include "linked_list.h"

/*
 * Hashmap module for storing (word -> count) pairs using separate chaining.
 * Keys are case-insensitive (normalized to lowercase) and stored in linked list buckets.
 * Designed for distributed word count (map/reduce/combine) applications.
 *
 * Hashing strategy:
 * - djb2 hash algorithm on lowercase key
 * - Bucket index = (hash % n_buckets)
 * - Collisions resolved via linked list chaining (see linked_list.h)
 *
 * Usage example:
 *   HashMap *map = hm_create(1024);
 *
 *   // Insert/increment "Example" and "EXAMPLE" -> count becomes 2
 *   hm_put_inc(map, "Example", 1);
 *   hm_put_inc(map, "EXAMPLE", 1);
 *
 *   // Retrieve as case-insensitive
 *   int count = 0;
 *   if (hm_get(map, "example", &count)) {
 *       printf("Word found with count: %d\n", count);
 *   }
 *
 *   // Iterate all entries
 *   hm_foreach(map, print_entry, NULL);
 *
 *   // Cleanup
 *   hm_free(map);
 */

typedef struct HashMap {
    size_t n_buckets;
    LLNode **buckets;      /* array of bucket heads */
} HashMap;

/*
 * hm_create - Create a new hashmap
 * @n_buckets: number of buckets (if 0, uses default of 1024)
 *
 * Allocates a new hashmap with the specified number of buckets.
 * All buckets start empty (NULL).
 * Returns pointer to HashMap, or NULL on allocation failure.
 */
HashMap *hm_create(size_t n_buckets);

/*
 * hm_free - Free a hashmap and all its entries
 * @hm: hashmap to free
 *
 * Frees all bucket lists (via ll_free), the buckets array, and the hashmap.
 * Safe to call with NULL.
 */
void hm_free(HashMap *hm);

/*
 * hm_entry_count - Get total number of entries in hashmap
 * @hm: hashmap
 *
 * Returns the total number of (key, count) pairs across all buckets.
 */
size_t hm_entry_count(const HashMap *hm);

/*
 * hm_put_inc - Insert or increment a key
 * @hm: hashmap
 * @key: key to insert or increment (case-insensitive)
 * @delta: amount to add to count (or initial count if new)
 *
 * If key exists: increments its count by delta.
 * If key not found: inserts new entry with count=delta.
 * Keys are stored in lowercase form.
 *
 * Returns 0 on success, -1 on allocation failure or invalid input.
 */
int hm_put_inc(HashMap *hm, const char *key, int delta);

/*
 * hm_set - Set count for a key exactly
 * @hm: hashmap
 * @key: key to set or insert (case-insensitive)
 * @count: exact count value to store
 *
 * If key exists: updates its count.
 * If key not found: inserts new entry.
 * Keys are stored in lowercase form.
 *
 * Returns 0 on success, -1 on allocation failure or invalid input.
 */
int hm_set(HashMap *hm, const char *key, int count);

/*
 * hm_get - Retrieve count for a key
 * @hm: hashmap
 * @key: key to search for (case-insensitive)
 * @out_count: pointer to store the count
 *
 * Returns 1 if key found (count stored in *out_count), 0 if not found.
 */
int hm_get(const HashMap *hm, const char *key, int *out_count);

/*
 * hm_foreach - Iterate over all entries in the hashmap
 * @hm: hashmap
 * @fn: callback function to invoke for each entry
 * @ctx: opaque context pointer passed to fn
 *
 * Calls fn(key, count, ctx) for every entry in the hashmap.
 * Iteration order is undefined (depends on hash distribution and bucket order).
 * Keys are stored in lowercase form.
 */
void hm_foreach(HashMap *hm,
                void (*fn)(const char *key, int count, void *ctx),
                void *ctx);

/*
 * hm_merge - Merge entries from source hashmap into destination
 * @dst: destination hashmap
 * @src: source hashmap (not modified)
 *
 * For each (key, count) in src, increments the key in dst by that count.
 * Used in map/reduce combine phase.
 *
 * Returns 0 on success, -1 if any allocation fails.
 */
int hm_merge(HashMap *dst, const HashMap *src);

/*
 * hm_clear - Clear all entries while keeping structure
 * @hm: hashmap to clear
 *
 * Frees all entries but keeps the hashmap structure intact for reuse.
 * Safe to call with NULL.
 */
void hm_clear(HashMap *hm);

#endif /* HASHMAP_H */
