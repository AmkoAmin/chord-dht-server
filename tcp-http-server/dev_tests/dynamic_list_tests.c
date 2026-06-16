#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ================== Globale Daten ==================

typedef struct DynamicResource {
    char *filename;              // z. B. "foo", "bar.json"
    char *content;               // Inhalt
    struct DynamicResource *next;
} DynamicResource;

DynamicResource *resource_list = NULL;
char body[8192];                 // globaler Buffer für Body (z.B. für GET)

// ================== Hilfsfunktionen ==================

static char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) {
        memcpy(p, s, len);
    }
    return p;
}

void reset_resources(void) {
    DynamicResource *curr = resource_list;
    while (curr) {
        DynamicResource *next = curr->next;
        free(curr->filename);
        free(curr->content);
        free(curr);
        curr = next;
    }
    resource_list = NULL;
    body[0] = '\0';
}

// ================== Zu testende Funktionen ==================
//
// Semantik wie in deinem Server:
//
// code == 1001 -> PUT /dynamic/<name>
// code == 1002 -> DELETE /dynamic/<name>
//
// Rückgaben:
//  - PUT neu      -> 201
//  - PUT update   -> 204
//  - DELETE ok    -> 201
//  - DELETE fehlt -> 404
//  - falscher Pfad /dynamic/...    -> 403
//  - Fehler / OOM -> 500 (optional)
//

int check_dynamic_resource(const char *uri, int code) {

    if (strncmp(uri, "/dynamic/", 9) != 0) {
        return 403; // falscher Pfad
    }

    const char *filename = uri + 9;  // alles hinter "/dynamic/"

    if (filename[0] == '\0') {
        return 404; // kein Name
    }

    if (code == 1001) {  // PUT
        DynamicResource *curr = resource_list;

        while (curr != NULL) {
            if (strcmp(curr->filename, filename) == 0) {
                // Update
                free(curr->content);
                curr->content = my_strdup(body);
                if (!curr->content) return 500;
                return 204;
            }
            curr = curr->next;
        }

        // Neu anlegen
        DynamicResource *new_node = malloc(sizeof(DynamicResource));
        if (!new_node) return 500;

        new_node->filename = my_strdup(filename);
        new_node->content  = my_strdup(body);
        if (!new_node->filename || !new_node->content) {
            free(new_node->filename);
            free(new_node->content);
            free(new_node);
            return 500;
        }

        new_node->next = resource_list;
        resource_list  = new_node;

        return 201;
    }

    if (code == 1002) {  // DELETE
        DynamicResource *curr = resource_list;
        DynamicResource *prev = NULL;

        while (curr != NULL) {
            if (strcmp(curr->filename, filename) == 0) {
                if (prev == NULL) {
                    resource_list = curr->next;
                } else {
                    prev->next = curr->next;
                }
                free(curr->filename);
                free(curr->content);
                free(curr);
                return 201;
            }
            prev = curr;
            curr = curr->next;
        }
        return 404;
    }

    return 500; // unerwarteter code
}

// GET-Funktion: sucht Ressource in Liste, kopiert content in globales body[]
// und gibt 200 oder 404 zurück.

int get_dynamic_resource(const char *uri) {

    if (strncmp(uri, "/dynamic/", 9) != 0) {
        return 404;
    }

    const char *filename = uri + 9;

    if (filename[0] == '\0') {
        return 404;
    }

    DynamicResource *curr = resource_list;
    while (curr != NULL) {
        if (strcmp(curr->filename, filename) == 0) {
            size_t len = strlen(curr->content);
            if (len >= sizeof(body)) {
                len = sizeof(body) - 1;
            }
            memcpy(body, curr->content, len);
            body[len] = '\0';
            return 200;
        }
        curr = curr->next;
    }

    return 404;
}

// ================== Test-Harness ==================

void print_list(void) {
    printf("Current list:\n");
    DynamicResource *curr = resource_list;
    while (curr) {
        printf("  filename='%s', content='%s'\n", curr->filename, curr->content);
        curr = curr->next;
    }
}

void run_test(const char *name, int condition) {
    printf("%s -> %s\n", name, condition ? "OK" : "FAIL");
}

int main(void) {
    int code;

    // ---------- Test 1: GET auf leere Liste -> 404 ----------
    reset_resources();
    code = get_dynamic_resource("/dynamic/foo");
    run_test("Test 1: GET fehlende Ressource -> 404", code == 404);

    // ---------- Test 2: PUT neu anlegen -> 201 ----------
    reset_resources();
    strcpy(body, "HelloWorld");
    code = check_dynamic_resource("/dynamic/foo", 1001);
    run_test("Test 2: PUT neu -> 201", code == 201);
    run_test("Test 2: Liste enthält foo", 
             resource_list != NULL && strcmp(resource_list->filename, "foo") == 0 &&
             strcmp(resource_list->content, "HelloWorld") == 0);

    // ---------- Test 3: GET bestehende Ressource -> 200 + Inhalt ----------
    body[0] = '\0';
    code = get_dynamic_resource("/dynamic/foo");
    run_test("Test 3: GET bestehende Ressource -> 200", code == 200);
    run_test("Test 3: body == 'HelloWorld'", strcmp(body, "HelloWorld") == 0);

    // ---------- Test 4: PUT Update -> 204, Inhalt überschrieben ----------
    strcpy(body, "NewContent");
    code = check_dynamic_resource("/dynamic/foo", 1001);
    run_test("Test 4: PUT Update -> 204", code == 204);
    run_test("Test 4: Inhalt wirklich 'NewContent'", 
             resource_list != NULL && strcmp(resource_list->content, "NewContent") == 0);

    // ---------- Test 5: DELETE bestehend -> 201 ----------
    code = check_dynamic_resource("/dynamic/foo", 1002);
    run_test("Test 5: DELETE bestehende Ressource -> 201", code == 201);
    run_test("Test 5: Liste leer", resource_list == NULL);

    // ---------- Test 6: DELETE fehlend -> 404 ----------
    code = check_dynamic_resource("/dynamic/foo", 1002);
    run_test("Test 6: DELETE nicht vorhandene Ressource -> 404", code == 404);

    // ---------- Test 7: Falscher Pfad -> 403 ----------
    reset_resources();
    strcpy(body, "Test");
    code = check_dynamic_resource("/wrong/foo", 1001);
    run_test("Test 7: PUT mit falschem Pfad -> 403", code == 403);

    return 0;
}
