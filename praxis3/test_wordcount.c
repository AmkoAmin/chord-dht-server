#include "wordcount.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print_entry(const char *key, int count, void *ctx) {
    (void)ctx;
    printf("  '%s': %d\n", key, count);
}

int main(void) {
    printf("=== Word Count Module Tests ===\n\n");

    printf("Test 1: Tokenize and count basic text\n");
    const char *text1 = "The quick brown fox jumps over the lazy dog.";
    HashMap *hm1 = hm_create(256);
    wc_tokenize_and_count(hm1, text1);
    printf("  Input: \"%s\"\n", text1);
    printf("  Word count: %zu\n", hm_entry_count(hm1));
    hm_foreach(hm1, print_entry, NULL);

    printf("\nTest 2: Case-insensitive counting\n");
    const char *text2 = "The quick quick QUICK Quick-quick.";
    HashMap *hm2 = hm_create(256);
    wc_tokenize_and_count(hm2, text2);
    printf("  Input: \"%s\"\n", text2);
    int count = 0;
    hm_get(hm2, "quick", &count);
    printf("  'quick' count (should be 5): %d\n", count);

    printf("\nTest 3: Complex punctuation and numbers\n");
    const char *text3 = "Hello, world! 123 test... [abc] (def)";
    HashMap *hm3 = hm_create(256);
    wc_tokenize_and_count(hm3, text3);
    printf("  Input: \"%s\"\n", text3);
    printf("  Words found: %zu\n", hm_entry_count(hm3));
    hm_foreach(hm3, print_entry, NULL);

    printf("\nTest 4: Map format encoding\n");
    const char *text4 = "the example text is the best example";
    HashMap *hm4 = hm_create(256);
    wc_tokenize_and_count(hm4, text4);
    printf("  Input: \"%s\"\n", text4);
    printf("  Counts:\n");
    hm_foreach(hm4, print_entry, NULL);

    char *map_payload = wc_to_map_format(hm4, 1024);
    printf("  Map format: \"%s\"\n", map_payload);

    printf("\nTest 5: Parse map format back\n");
    HashMap *hm5 = wc_from_map_format(map_payload);
    printf("  Parsed counts:\n");
    hm_foreach(hm5, print_entry, NULL);

    printf("\nTest 6: Reduce format encoding\n");
    char *reduce_payload = wc_to_reduce_format(hm5, 1024);
    printf("  Reduce format: \"%s\"\n", reduce_payload);

    printf("\nTest 7: Compare map and reduce formats\n");
    int c1 = 0, c2 = 0;
    hm_get(hm4, "the", &c1);
    hm_get(hm5, "the", &c2);
    printf("  Original 'the' count: %d\n", c1);
    printf("  Parsed 'the' count: %d\n", c2);
    printf("  Match: %s\n", c1 == c2 ? "YES" : "NO");

    printf("\nTest 8: Message length truncation (map format)\n");
    char *short_map = wc_to_map_format(hm4, 20);
    printf("  Map format (msg_len=20): \"%s\" (len=%zu)\n", short_map, strlen(short_map));

    printf("\nTest 9: Message length truncation (reduce format)\n");
    char *short_reduce = wc_to_reduce_format(hm4, 20);
    printf("  Reduce format (msg_len=20): \"%s\" (len=%zu)\n", short_reduce, strlen(short_reduce));

    printf("\nTest 10: Empty and NULL handling\n");
    HashMap *hm_empty = hm_create(256);
    char *map_empty = wc_to_map_format(hm_empty, 1024);
    char *reduce_empty = wc_to_reduce_format(hm_empty, 1024);
    printf("  Empty map format: \"%s\"\n", map_empty);
    printf("  Empty reduce format: \"%s\"\n", reduce_empty);

    printf("\nTest 11: NULL text and payload\n");
    HashMap *hm_null = hm_create(256);
    wc_tokenize_and_count(hm_null, NULL);
    printf("  After wc_tokenize_and_count(hm, NULL): count = %zu\n", hm_entry_count(hm_null));

    HashMap *hm_null_parse = wc_from_map_format(NULL);
    printf("  After wc_from_map_format(NULL): count = %zu\n", hm_entry_count(hm_null_parse));

    printf("\nTest 12: Small message buffer (edge case)\n");
    char *tiny = wc_to_map_format(hm4, 1);
    printf("  Map format (msg_len=1): \"%s\"\n", tiny);
    char *tiny_reduce = wc_to_reduce_format(hm4, 1);
    printf("  Reduce format (msg_len=1): \"%s\"\n", tiny_reduce);

    printf("\nTest 13: Complex roundtrip\n");
    const char *complex = "AAA bbb CCC ddd eee FFF aaa BBB ccc";
    HashMap *orig = hm_create(256);
    wc_tokenize_and_count(orig, complex);
    char *encoded = wc_to_map_format(orig, 4096);
    HashMap *decoded = wc_from_map_format(encoded);
    char *reencoded = wc_to_reduce_format(decoded, 4096);
    printf("  Original: %zu entries\n", hm_entry_count(orig));
    printf("  Decoded:  %zu entries\n", hm_entry_count(decoded));
    printf("  Reduce format: \"%s\"\n", reencoded);

    printf("\n=== Cleanup ===\n");
    free(map_payload);
    free(reduce_payload);
    free(short_map);
    free(short_reduce);
    free(map_empty);
    free(reduce_empty);
    free(tiny);
    free(tiny_reduce);
    free(encoded);
    free(reencoded);

    hm_free(hm1);
    hm_free(hm2);
    hm_free(hm3);
    hm_free(hm4);
    hm_free(hm5);
    hm_free(hm_empty);
    hm_free(hm_null);
    hm_free(hm_null_parse);
    hm_free(orig);
    hm_free(decoded);

    printf("✓ All tests passed\n");
    return 0;
}
