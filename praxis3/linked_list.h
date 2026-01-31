#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stddef.h>

/*
 * Singly linked list module for word-count hashmap chaining.
 *
 * This module provides a list-based bucket for storing (word, count) pairs
 * where keys are case-insensitive and stored in lowercase form.
 * Designed for use in distributed wordcount (map/reduce) applications.
 *
 * Usage example:
 *   LLNode *bucket = NULL;
 *
 *   // Insert/increment "Example" twice -> count becomes 2
 *   ll_put_inc(&bucket, "Example", 1);
 *   ll_put_inc(&bucket, "EXAMPLE", 1);
 *
 *   // Retrieve as lowercase
 *   int count = 0;
 *   if (ll_get(bucket, "example", &count)) {
 *       printf("Word found with count: %d\n", count);
 *   }
 *
 *   // Iterate all entries
 *   ll_foreach(bucket, print_entry, NULL);
 *
 *   // Cleanup
 *   ll_free(bucket);
 */

typedef struct LLNode {
    char *key;          /* heap allocated, stored in lowercase */
    int count;
    struct LLNode *next;
} LLNode;

/*
 * ll_strdup_lower - Create a lowercase copy of a string
 * @s: input string (NULL treated as empty string)
 *
 * Returns a newly allocated string converted to lowercase (ASCII only).
 * Caller must free the returned pointer.
 * Returns NULL only on allocation failure.
 */
char *ll_strdup_lower(const char *s);

/*
 * ll_find - Find a node by case-insensitive key match
 * @head: head of the linked list
 * @key: key to search for (normalized before comparison)
 *
 * Returns pointer to the node if found, NULL otherwise.
 */
LLNode *ll_find(LLNode *head, const char *key);

/*
 * ll_get - Retrieve count for a given key
 * @head: head of the linked list
 * @key: key to search for
 * @out_count: pointer to store the count
 *
 * Returns 1 if key found (count stored in *out_count), 0 if not found.
 */
int ll_get(LLNode *head, const char *key, int *out_count);

/*
 * ll_put_inc - Insert a new entry or increment existing count
 * @head: pointer to head of the linked list
 * @key: key to insert or increment (case-insensitive)
 * @delta: amount to add to count (or initial count if new)
 *
 * If key exists: increments its count by delta.
 * If key not found: inserts new node at head with count=delta.
 *
 * Returns 0 on success, -1 on allocation failure.
 */
int ll_put_inc(LLNode **head, const char *key, int delta);

/*
 * ll_set - Set count for a key exactly
 * @head: pointer to head of the linked list
 * @key: key to set or insert (case-insensitive)
 * @count: exact count value to store
 *
 * If key exists: updates its count.
 * If key not found: inserts new node at head.
 *
 * Returns 0 on success, -1 on allocation failure.
 */
int ll_set(LLNode **head, const char *key, int count);

/*
 * ll_remove - Remove a node by key
 * @head: pointer to head of the linked list
 * @key: key to remove (case-insensitive)
 *
 * Returns 1 if node was removed, 0 if key not found.
 * Frees the removed node and its key.
 */
int ll_remove(LLNode **head, const char *key);

/*
 * ll_free - Free all nodes and their keys
 * @head: head of the linked list
 *
 * Recursively (or iteratively) frees all nodes and keys.
 */
void ll_free(LLNode *head);

/*
 * ll_length - Get number of nodes in the list
 * @head: head of the linked list
 *
 * Returns the count of nodes.
 */
size_t ll_length(LLNode *head);

/*
 * ll_foreach - Iterate over all entries in the list
 * @head: head of the linked list
 * @fn: callback function to invoke for each entry
 * @ctx: opaque context pointer passed to fn
 *
 * The callback receives (key, count, ctx).
 * Nodes are visited in list order (LIFO order of insertion).
 */
void ll_foreach(LLNode *head,
                void (*fn)(const char *key, int count, void *ctx),
                void *ctx);

#endif /* LINKED_LIST_H */
