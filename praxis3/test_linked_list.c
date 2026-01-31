#include "linked_list.h"
#include <stdio.h>
#include <string.h>

/* Helper callback for foreach */
void print_entry(const char *key, int count, void *ctx) {
    (void)ctx;  /* unused */
    printf("  key='%s', count=%d\n", key, count);
}

int main(void) {
    LLNode *bucket = NULL;

    printf("Test 1: Insert 'Example' twice with case variations\n");
    int result = ll_put_inc(&bucket, "Example", 1);
    printf("  ll_put_inc(bucket, 'Example', 1) = %d\n", result);

    result = ll_put_inc(&bucket, "EXAMPLE", 1);
    printf("  ll_put_inc(bucket, 'EXAMPLE', 1) = %d\n", result);

    printf("\nTest 2: Retrieve count as lowercase 'example'\n");
    int count = 0;
    result = ll_get(bucket, "example", &count);
    printf("  ll_get(bucket, 'example', &count) = %d, count = %d\n", result, count);

    printf("\nTest 3: Add more words\n");
    ll_put_inc(&bucket, "Hello", 2);
    ll_put_inc(&bucket, "World", 3);
    printf("  Added 'Hello' with +2, 'World' with +3\n");

    printf("\nTest 4: List length\n");
    size_t len = ll_length(bucket);
    printf("  ll_length(bucket) = %zu\n", len);

    printf("\nTest 5: Iterate all entries\n");
    ll_foreach(bucket, print_entry, NULL);

    printf("\nTest 6: Update existing key with ll_set\n");
    ll_set(&bucket, "hello", 10);
    result = ll_get(bucket, "Hello", &count);
    printf("  After ll_set(bucket, 'hello', 10): count = %d\n", count);

    printf("\nTest 7: Remove a key\n");
    result = ll_remove(&bucket, "WORLD");
    printf("  ll_remove(bucket, 'WORLD') = %d\n", result);
    len = ll_length(bucket);
    printf("  ll_length(bucket) = %zu\n", len);

    printf("\nTest 8: Empty string key\n");
    ll_put_inc(&bucket, "", 1);
    result = ll_get(bucket, "", &count);
    printf("  ll_get(bucket, \"\", &count) = %d, count = %d\n", result, count);

    printf("\nTest 9: Final iteration\n");
    ll_foreach(bucket, print_entry, NULL);

    printf("\nTest 10: Cleanup\n");
    ll_free(bucket);
    printf("  ll_free(bucket) completed\n");

    return 0;
}
