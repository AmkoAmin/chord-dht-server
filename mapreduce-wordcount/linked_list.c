#include "linked_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Helper: Convert a single character to lowercase (ASCII only) */
static char ascii_tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - ('A' - 'a');
    }
    return c;
}

/*
 * ll_strdup_lower - Create a lowercase copy of a string
 */
char *ll_strdup_lower(const char *s) {
    /* Treat NULL as empty string */
    if (s == NULL) {
        s = "";
    }

    size_t len = strlen(s);
    char *result = (char *)malloc(len + 1);
    if (result == NULL) {
        return NULL;
    }

    for (size_t i = 0; i <= len; i++) {
        result[i] = ascii_tolower(s[i]);
    }

    return result;
}

/*
 * ll_find - Find a node by case-insensitive key match
 */
LLNode *ll_find(LLNode *head, const char *key) {
    if (key == NULL) {
        key = "";
    }

    char *key_lower = ll_strdup_lower(key);
    if (key_lower == NULL) {
        return NULL;  /* allocation failure, treat as not found */
    }

    for (LLNode *node = head; node != NULL; node = node->next) {
        if (strcmp(node->key, key_lower) == 0) {
            free(key_lower);
            return node;
        }
    }

    free(key_lower);
    return NULL;
}

/*
 * ll_get - Retrieve count for a given key
 */
int ll_get(LLNode *head, const char *key, int *out_count) {
    if (out_count == NULL) {
        return 0;
    }

    LLNode *node = ll_find(head, key);
    if (node != NULL) {
        *out_count = node->count;
        return 1;
    }

    return 0;
}

/*
 * ll_put_inc - Insert a new entry or increment existing count
 */
int ll_put_inc(LLNode **head, const char *key, int delta) {
    if (head == NULL) {
        return -1;
    }

    if (key == NULL) {
        key = "";
    }

    LLNode *node = ll_find(*head, key);
    if (node != NULL) {
        /* Key exists: increment count */
        node->count += delta;
        return 0;
    }

    /* Key not found: create new node at head */
    char *key_lower = ll_strdup_lower(key);
    if (key_lower == NULL) {
        return -1;
    }

    LLNode *new_node = (LLNode *)malloc(sizeof(LLNode));
    if (new_node == NULL) {
        free(key_lower);
        return -1;
    }

    new_node->key = key_lower;
    new_node->count = delta;
    new_node->next = *head;
    *head = new_node;

    return 0;
}

/*
 * ll_set - Set count for a key exactly
 */
int ll_set(LLNode **head, const char *key, int count) {
    if (head == NULL) {
        return -1;
    }

    if (key == NULL) {
        key = "";
    }

    LLNode *node = ll_find(*head, key);
    if (node != NULL) {
        /* Key exists: set count */
        node->count = count;
        return 0;
    }

    /* Key not found: create new node at head */
    char *key_lower = ll_strdup_lower(key);
    if (key_lower == NULL) {
        return -1;
    }

    LLNode *new_node = (LLNode *)malloc(sizeof(LLNode));
    if (new_node == NULL) {
        free(key_lower);
        return -1;
    }

    new_node->key = key_lower;
    new_node->count = count;
    new_node->next = *head;
    *head = new_node;

    return 0;
}

/*
 * ll_remove - Remove a node by key
 */
int ll_remove(LLNode **head, const char *key) {
    if (head == NULL || *head == NULL) {
        return 0;
    }

    if (key == NULL) {
        key = "";
    }

    char *key_lower = ll_strdup_lower(key);
    if (key_lower == NULL) {
        return 0;  /* allocation failure, treat as not found */
    }

    /* Check if head matches */
    if (strcmp((*head)->key, key_lower) == 0) {
        LLNode *temp = *head;
        *head = (*head)->next;
        free(temp->key);
        free(temp);
        free(key_lower);
        return 1;
    }

    /* Search in rest of list */
    for (LLNode *current = *head; current->next != NULL; current = current->next) {
        if (strcmp(current->next->key, key_lower) == 0) {
            LLNode *temp = current->next;
            current->next = temp->next;
            free(temp->key);
            free(temp);
            free(key_lower);
            return 1;
        }
    }

    free(key_lower);
    return 0;
}

/*
 * ll_free - Free all nodes and their keys
 */
void ll_free(LLNode *head) {
    LLNode *current = head;
    while (current != NULL) {
        LLNode *next = current->next;
        free(current->key);
        free(current);
        current = next;
    }
}

/*
 * ll_length - Get number of nodes in the list
 */
size_t ll_length(LLNode *head) {
    size_t count = 0;
    for (LLNode *node = head; node != NULL; node = node->next) {
        count++;
    }
    return count;
}

/*
 * ll_foreach - Iterate over all entries in the list
 */
void ll_foreach(LLNode *head,
                void (*fn)(const char *key, int count, void *ctx),
                void *ctx) {
    if (fn == NULL) {
        return;
    }

    for (LLNode *node = head; node != NULL; node = node->next) {
        fn(node->key, node->count, ctx);
    }
}
