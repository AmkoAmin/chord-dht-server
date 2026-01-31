#include "wordcount.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_failed = 0;

/*
 * Helper: Assert that a word has the expected count
 */
static void assert_count(const char *test_name, HashMap *hm, const char *word, int expected) {
    int actual = 0;
    int found = hm_get(hm, word, &actual);
    
    if (!found) {
        actual = 0;
    }
    
    if (actual == expected) {
        printf("  [PASS] %s: '%s' = %d\n", test_name, word, actual);
    } else {
        printf("  [FAIL] %s: '%s' expected %d, got %d\n", test_name, word, expected, actual);
        test_failed = 1;
    }
}

int main(void) {
    printf("=== Word Count Demo / Test Program ===\n\n");
    
    /* Test A: Assignment example */
    printf("Test A: Assignment example text\n");
    const char *example_text = "The example text is the best example.";
    printf("  Input: \"%s\"\n", example_text);
    
    HashMap *hm = hm_create(1024);
    if (hm == NULL) {
        fprintf(stderr, "Failed to create hashmap\n");
        return 1;
    }
    
    wc_tokenize_and_count(hm, example_text);
    
    char *map_payload = wc_to_map_format(hm, 1500);
    if (map_payload == NULL) {
        fprintf(stderr, "Failed to create map payload\n");
        hm_free(hm);
        return 1;
    }
    printf("  Map format: \"%s\"\n", map_payload);
    
    HashMap *hm2 = wc_from_map_format(map_payload);
    if (hm2 == NULL) {
        fprintf(stderr, "Failed to parse map format\n");
        free(map_payload);
        hm_free(hm);
        return 1;
    }
    
    char *reduce_payload = wc_to_reduce_format(hm2, 1500);
    if (reduce_payload == NULL) {
        fprintf(stderr, "Failed to create reduce payload\n");
        hm_free(hm2);
        free(map_payload);
        hm_free(hm);
        return 1;
    }
    printf("  Reduce format: \"%s\"\n\n", reduce_payload);
    
    printf("  Verifying parsed counts:\n");
    assert_count("TestA", hm2, "the", 2);
    assert_count("TestA", hm2, "example", 2);
    assert_count("TestA", hm2, "text", 1);
    assert_count("TestA", hm2, "is", 1);
    assert_count("TestA", hm2, "best", 1);
    
    free(map_payload);
    free(reduce_payload);
    hm_free(hm);
    hm_free(hm2);
    
    /* Test B: Robustness with junk characters */
    printf("\nTest B: Robustness - junk characters between pairs\n");
    const char *payload_junk = "the11,example11!!text1";
    printf("  Input: \"%s\"\n", payload_junk);
    
    HashMap *hm3 = wc_from_map_format(payload_junk);
    if (hm3 == NULL) {
        fprintf(stderr, "Failed to parse map format with junk\n");
        return 1;
    }
    
    printf("  Verifying parsed counts:\n");
    assert_count("TestB", hm3, "the", 2);
    assert_count("TestB", hm3, "example", 2);
    assert_count("TestB", hm3, "text", 1);
    
    hm_free(hm3);
    
    /* Test C: Leading ones ignored */
    printf("\nTest C: Robustness - leading '1's ignored\n");
    const char *payload_leading = "111the11";
    printf("  Input: \"%s\"\n", payload_leading);
    
    HashMap *hm4 = wc_from_map_format(payload_leading);
    if (hm4 == NULL) {
        fprintf(stderr, "Failed to parse map format with leading ones\n");
        return 1;
    }
    
    printf("  Verifying parsed counts:\n");
    assert_count("TestC", hm4, "the", 2);
    
    hm_free(hm4);
    
    /* Final summary */
    printf("\n=== Summary ===\n");
    if (test_failed) {
        printf("SOME TESTS FAILED\n");
        return 1;
    } else {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
}
