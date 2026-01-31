#ifndef WORDCOUNT_H
#define WORDCOUNT_H

#include <stddef.h>
#include "hashmap.h"

/*
 * Word count module for "Distributed Word Count" assignment.
 * Provides pure wordcount logic: tokenization, map-format encoding, and reduce-format encoding.
 * Works with HashMap to count words (case-insensitive) and format output for map/reduce phases.
 *
 * Word definition:
 * - Any contiguous sequence of ASCII letters [A-Z][a-z]
 * - Non-letter characters (spaces, punctuation, digits, hyphen, etc.) split words
 * - Counting is case-insensitive; words stored in lowercase
 *
 * Format definitions:
 * - Map format: concatenation of (word + '1' repeated count times), no separators
 *   Example: "the11example11text1" means "the"->2, "example"->2, "text"->1
 * - Reduce format: concatenation of (word + decimal count), no separators
 *   Example: "the2example2text1" means same counts
 *
 * Usage example:
 *   const char *text = "The example text is the best example.";
 *
 *   // Tokenize and count
 *   HashMap *map = hm_create(256);
 *   wc_tokenize_and_count(map, text);
 *   // map now contains: "the"->2, "example"->2, "text"->1, "is"->1, "best"->1
 *
 *   // Convert to map format (word + repeated '1's)
 *   char *map_payload = wc_to_map_format(map, 1024);
 *   // Returns something like: "the11example11text1is1best1" (order varies)
 *   // Send map_payload over network
 *
 *   // On reducer: parse map format back to HashMap
 *   HashMap *reduced = wc_from_map_format(map_payload);
 *   // reduced has same (word, count) pairs
 *
 *   // Convert to reduce format (word + decimal count)
 *   char *reduce_payload = wc_to_reduce_format(reduced, 1024);
 *   // Returns something like: "the2example2text1is1best1"
 *
 *   free(map_payload);
 *   free(reduce_payload);
 *   hm_free(map);
 *   hm_free(reduced);
 */

/*
 * wc_tokenize_and_count - Tokenize text and count words in a hashmap
 * @hm: hashmap to store word counts
 * @text: input text to tokenize (NULL treated as empty)
 *
 * Tokenizes the text into words (ASCII letters only), converts to lowercase,
 * and increments the count in the hashmap for each word.
 * Non-letter characters are treated as word separators.
 *
 * Behavior:
 * - NULL text is a no-op
 * - Words are stored in lowercase
 * - Each word occurrence increments count by 1
 * - No memory leaks; uses hashmap's allocation
 */
void wc_tokenize_and_count(HashMap *hm, const char *text);

/*
 * wc_to_map_format - Convert hashmap to map-phase output format
 * @hm: hashmap with word counts
 * @msg_len: maximum output size (including NUL terminator)
 *
 * Returns a newly allocated string in map format:
 * Concatenation of (word + '1' repeated count times) for all entries.
 * No separators. Example: "the11example11text1" means "the"->2, "example"->2, "text"->1.
 *
 * Output is:
 * - NUL-terminated
 * - At most msg_len-1 bytes (+ NUL)
 * - Truncated safely if needed to fit
 * - Returns allocated "" (empty string) if msg_len < 2
 *
 * Caller must free the returned pointer.
 */
char *wc_to_map_format(const HashMap *hm, size_t msg_len);

/*
 * wc_from_map_format - Parse map-format output back to a hashmap
 * @payload: map-format string (NULL treated as empty)
 *
 * Parses the map-format payload and creates a new hashmap with the counts.
 * For each word found, counts the number of consecutive '1' characters as the count.
 * Robust to unexpected characters; skips them.
 *
 * Returns:
 * - A newly allocated HashMap (use hm_create(1024))
 * - Caller must free with hm_free()
 * - If payload is NULL or empty, returns an empty hashmap
 */
HashMap *wc_from_map_format(const char *payload);

/*
 * wc_to_reduce_format - Convert hashmap to reduce-phase output format
 * @hm: hashmap with word counts
 * @msg_len: maximum output size (including NUL terminator)
 *
 * Returns a newly allocated string in reduce format:
 * Concatenation of (word + decimal count) for all entries.
 * No separators. Example: "the2example2text1" means "the"->2, "example"->2, "text"->1.
 *
 * Output is:
 * - NUL-terminated
 * - At most msg_len-1 bytes (+ NUL)
 * - Truncated safely if needed to fit
 * - Returns allocated "" (empty string) if msg_len < 2
 *
 * Caller must free the returned pointer.
 */
char *wc_to_reduce_format(const HashMap *hm, size_t msg_len);

#endif /* WORDCOUNT_H */
