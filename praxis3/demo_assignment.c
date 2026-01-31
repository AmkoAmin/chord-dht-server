#include "wordcount.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Demonstration of the assignment use case:
 * Simulate distributed wordcount with map/reduce pattern
 */

int main(void) {
    printf("=== Distributed Word Count Assignment Demo ===\n\n");

    /* Simulating the assignment scenario from the description */
    printf("Scenario: Process text distributed across multiple nodes\n");
    printf("Then aggregate and reduce on a coordinator\n\n");

    /* NODE 1: Map phase - process chunk 1 */
    printf("MAP NODE 1: Process chunk 1\n");
    const char *chunk1 = "The quick brown fox jumps over the lazy dog. "
                         "The fox is quick.";
    HashMap *map1 = hm_create(256);
    wc_tokenize_and_count(map1, chunk1);
    printf("  Tokenized and counted words\n");

    /* Encode for transmission */
    char *encoded1 = wc_to_map_format(map1, 1024);
    printf("  Map format payload: %s\n", encoded1);
    printf("  Payload size: %zu bytes\n\n", strlen(encoded1));

    /* NODE 2: Map phase - process chunk 2 */
    printf("MAP NODE 2: Process chunk 2\n");
    const char *chunk2 = "The dog is lazy. The quick fox jumps. "
                         "Fox and dog are animals.";
    HashMap *map2 = hm_create(256);
    wc_tokenize_and_count(map2, chunk2);
    printf("  Tokenized and counted words\n");

    char *encoded2 = wc_to_map_format(map2, 1024);
    printf("  Map format payload: %s\n", encoded2);
    printf("  Payload size: %zu bytes\n\n", strlen(encoded2));

    /* COORDINATOR: Reduce phase */
    printf("COORDINATOR: Combine and reduce results\n");

    /* Create the reduce container */
    HashMap *reduce_map = hm_create(512);

    /* Receive from node 1 and merge */
    printf("  Receiving from MAP NODE 1...\n");
    HashMap *from_node1 = wc_from_map_format(encoded1);
    hm_merge(reduce_map, from_node1);

    /* Receive from node 2 and merge */
    printf("  Receiving from MAP NODE 2...\n");
    HashMap *from_node2 = wc_from_map_format(encoded2);
    hm_merge(reduce_map, from_node2);

    printf("  Total unique words after reduce: %zu\n", hm_entry_count(reduce_map));

    /* Encode final result in reduce format */
    printf("\n  Final word counts (reduce format):\n");
    char *final_result = wc_to_reduce_format(reduce_map, 1024);
    printf("    %s\n", final_result);

    /* Display decoded results */
    printf("\n  Decoded final counts:\n");
    int count;
    const char *test_words[] = {"the", "fox", "quick", "dog", "lazy", "animals"};
    for (int i = 0; i < 6; i++) {
        if (hm_get(reduce_map, test_words[i], &count)) {
            printf("    '%s': %d\n", test_words[i], count);
        }
    }

    printf("\n=== Verification ===\n");
    int fox_count, the_count, dog_count;
    hm_get(reduce_map, "fox", &fox_count);
    hm_get(reduce_map, "the", &the_count);
    hm_get(reduce_map, "dog", &dog_count);
    printf("'fox' appears %d times (expected 4: 2 from chunk1 + 2 from chunk2)\n", fox_count);
    printf("'the' appears %d times (expected 4: 2 from chunk1 + 2 from chunk2)\n", the_count);
    printf("'dog' appears %d times (expected 3: 2 from chunk1 + 1 from chunk2)\n", dog_count);

    printf("\n=== Cleanup ===\n");
    free(encoded1);
    free(encoded2);
    free(final_result);
    hm_free(map1);
    hm_free(map2);
    hm_free(reduce_map);
    hm_free(from_node1);
    hm_free(from_node2);
    printf("✓ All resources freed\n");

    return 0;
}
