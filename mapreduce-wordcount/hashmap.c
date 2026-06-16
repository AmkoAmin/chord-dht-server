#include "hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Default number of buckets if not specified */
#define DEFAULT_N_BUCKETS 1024

/*
 * djb2_hash - Dan Bernstein's hash function for strings
 * Computes hash on lowercase bytes (case-insensitive)
 * @key: input string (NULL treated as empty)
 * Returns the hash value
 */
static uint64_t djb2_hash(const char *key) {
    if (key == NULL) {
        key = "";
    }

    uint64_t hash = 5381;
    unsigned char c;

    while ((c = (unsigned char)*key++)) {
        /* Convert to lowercase (ASCII only) */
        if (c >= 'A' && c <= 'Z') {
            c = c - ('A' - 'a');
        }
        /* hash = hash * 33 + c */
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

/*
 * bucket_index - Get bucket index for a key
 * @hm: hashmap
 * @key: key to hash
 * Returns bucket index in range [0, n_buckets)
 */
static size_t bucket_index(const HashMap *hm, const char *key) {
    if (hm == NULL || hm->n_buckets == 0) {
        return 0;
    }
    return djb2_hash(key) % hm->n_buckets;
}

/*
 * hm_create - Create a new hashmap
 */
HashMap *hm_create(size_t n_buckets) {
    if (n_buckets == 0) {
        n_buckets = DEFAULT_N_BUCKETS;
    }

    HashMap *hm = (HashMap *)malloc(sizeof(HashMap));
    if (hm == NULL) {
        return NULL;
    }

    hm->buckets = (LLNode **)calloc(n_buckets, sizeof(LLNode *));
    if (hm->buckets == NULL) {
        free(hm);
        return NULL;
    }

    hm->n_buckets = n_buckets;
    return hm;
}

/*
 * hm_free - Free a hashmap and all its entries
 */
void hm_free(HashMap *hm) {
    if (hm == NULL) {
        return;
    }

    if (hm->buckets != NULL) {
        for (size_t i = 0; i < hm->n_buckets; i++) {
            ll_free(hm->buckets[i]);
        }
        free(hm->buckets);
    }

    free(hm);
}

/*
 * hm_entry_count - Get total number of entries
 */
size_t hm_entry_count(const HashMap *hm) {
    if (hm == NULL || hm->buckets == NULL) {
        return 0;
    }

    size_t total = 0;
    for (size_t i = 0; i < hm->n_buckets; i++) {
        total += ll_length(hm->buckets[i]);
    }

    return total;
}

/*
 * hm_put_inc - Insert or increment a key
 */
int hm_put_inc(HashMap *hm, const char *key, int delta) {
    if (hm == NULL || hm->buckets == NULL) {
        return -1;
    }

    if (key == NULL) {
        key = "";
    }

    size_t idx = bucket_index(hm, key);
    return ll_put_inc(&hm->buckets[idx], key, delta);
}

/*
 * hm_set - Set count for a key exactly
 */
int hm_set(HashMap *hm, const char *key, int count) {
    if (hm == NULL || hm->buckets == NULL) {
        return -1;
    }

    if (key == NULL) {
        key = "";
    }

    size_t idx = bucket_index(hm, key);
    return ll_set(&hm->buckets[idx], key, count);
}

/*
 * hm_get - Retrieve count for a key
 */
int hm_get(const HashMap *hm, const char *key, int *out_count) {
    if (hm == NULL || hm->buckets == NULL) {
        return 0;
    }

    if (out_count == NULL) {
        return 0;
    }

    if (key == NULL) {
        key = "";
    }

    size_t idx = bucket_index(hm, key);
    return ll_get(hm->buckets[idx], key, out_count);
}

/*
 * hm_foreach - Iterate over all entries
 */
void hm_foreach(HashMap *hm,
                void (*fn)(const char *key, int count, void *ctx),
                void *ctx) {
    if (hm == NULL || hm->buckets == NULL || fn == NULL) {
        return;
    }

    for (size_t i = 0; i < hm->n_buckets; i++) {
        ll_foreach(hm->buckets[i], fn, ctx);
    }
}

/*
 * hm_merge - Merge entries from source into destination
 */
int hm_merge(HashMap *dst, const HashMap *src) {
    if (dst == NULL || dst->buckets == NULL) {
        return -1;
    }

    if (src == NULL || src->buckets == NULL) {
        return 0;  /* Nothing to merge */
    }

    for (size_t i = 0; i < src->n_buckets; i++) {
        for (LLNode *node = src->buckets[i]; node != NULL; node = node->next) {
            int result = hm_put_inc(dst, node->key, node->count);
            if (result != 0) {
                return -1;
            }
        }
    }

    return 0;
}

/*
 * hm_clear - Clear all entries
 */
void hm_clear(HashMap *hm) {
    if (hm == NULL || hm->buckets == NULL) {
        return;
    }

    for (size_t i = 0; i < hm->n_buckets; i++) {
        ll_free(hm->buckets[i]);
        hm->buckets[i] = NULL;
    }
}
