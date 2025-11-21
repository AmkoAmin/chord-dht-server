#include <stdio.h>
#include <string.h>
#include <stdlib.h>   // malloc, free, strdup
#include <stdbool.h>

// ----------------- Globale Daten aus dem Server-Code -----------------

char global_uri[256];
char dynamic_uri[256];
char body[8192];

typedef struct DynamicResource {
    char* filename;          // z. B. "foo", "bar.json", ...
    char* content;           // Inhalt der Datei/Ressource
    struct DynamicResource *next;   // Zeiger auf das nächste Element
} DynamicResource;

DynamicResource* resource_list = NULL;

// ----------------- Helferfunktionen aus dem Server-Code -----------------

static char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

int find_crlfcrlf(const char *buf, int len) {
    for (int i = 0; i <= len - 4; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' &&
            buf[i+2] == '\r' && buf[i+3] == '\n') {
            return i;
        }
    }
    return -1;
}

int check_first_line(const char *buf, int len) {

    // Suche erstes CRLF
    int i = 0;
    while (i + 1 < len && !(buf[i] == '\r' && buf[i+1] == '\n')) {
        i++;
    }

    if (i + 1 >= len) {
        // Kein vollständiges CRLF → keine Startzeile
        return 400;
    }

    int line_len = i;
    if (line_len <= 0 || line_len > 256) {
        return 400;
    }

    // Zeile kopieren
    char line[257];
    memcpy(line, buf, line_len);
    line[line_len] = '\0';

    // Tokens extrahieren: <Method> <URI> <HTTP-Version>
    char method[16];
    char uri[256];
    char version[16];

    if (sscanf(line, "%15s %255s %15s", method, uri, version) != 3) {
        return 400;
    }

    // HTTP-Version prüfen
    if (strcmp(version, "HTTP/1.1") != 0) {
        return 400;
    }

    int pos = 0;
    while (pos + 1 < len && !(buf[pos] == '\r' && buf[pos+1] == '\n')) {
        pos++;
    }

    // Überspringe das CRLF der Startzeile
    pos += 2;

    // Jetzt stehen wir am Anfang der Header
    while (pos + 1 < len) {

        int line_start = pos;

        // Finde nächstes CRLF = Ende dieser Headerzeile
        while (pos + 1 < len && !(buf[pos] == '\r' && buf[pos+1] == '\n')) {
            pos++;
        }

        if (pos + 1 >= len) {
            // kein vollständiges CRLF → kaputt
            return 400;
        }

        int hlen = pos - line_start;

        // Leerzeile? → Header zu Ende → fertig
        if (hlen == 0) {
            break;
        }

        // Headerzeile in temporären Buffer kopieren
        char hline[512];
        if (hlen >= 511) {
            return 400; // Header viel zu lang
        }
        memcpy(hline, buf + line_start, hlen);
        hline[hlen] = '\0';

        // Prüfen: Key: Value
        char *colon = strchr(hline, ':');
        if (!colon) {
            return 400; // kein ':' gefunden
        }

        // Key darf nicht leer sein
        if (colon == hline) {
            return 400;
        }

        // Muss ": " sein
        if (*(colon + 1) != ' ') {
            return 400;
        }

        // Value darf nicht leer sein
        if (*(colon + 2) == '\0') {
            return 400;
        }

        // Nächste Headerzeile nach dem CRLF
        pos += 2;
    }

    // Methode bestimmen
    if (strcmp(method, "GET") == 0) {
        strcpy(global_uri, uri);
        return 1000; // gültiger GET-Request
    } else if (strcmp(method, "PUT") == 0) {
        strcpy(dynamic_uri, uri);
        return 1001;
    } else if (strcmp(method, "DELETE") == 0) {
        strcpy(dynamic_uri, uri);
        return 1002;
    } else {
        return 501; // gültig, aber nicht GET/PUT/DELETE
    }
}

int check_after_first_line(const char *buf, int len, int code_from_first_line) {
    // 1. Ende der Startzeile finden (erstes CRLF)
    int pos = 0;
    while (pos + 1 < len && !(buf[pos] == '\r' && buf[pos+1] == '\n')) {
        pos++;
    }

    if (pos + 1 >= len) {
        // sollte eigentlich nicht passieren, wenn check_first_line ok war
        return 400;
    }

    // hinter das CRLF der Startzeile springen
    pos += 2;

    // 2. Headerzeilen prüfen
    while (pos + 1 < len) {
        int line_start = pos;

        // bis zum nächsten CRLF
        while (pos + 1 < len && !(buf[pos] == '\r' && buf[pos+1] == '\n')) {
            pos++;
        }

        if (pos + 1 >= len) {
            // kein vollständiges CRLF → kaputt
            return 400;
        }

        int line_len = pos - line_start;

        // Leere Zeile → Ende der Header
        if (line_len == 0) {
            break;
        }

        // Headerzeile kopieren
        if (line_len >= 511) {
            return 400; // zu lang
        }

        char hline[512];
        memcpy(hline, buf + line_start, line_len);
        hline[line_len] = '\0';

        // Muster: "Key: Value"
        char *colon = strchr(hline, ':');
        if (!colon) {
            return 400;
        }

        // Key darf nicht leer sein
        if (colon == hline) {
            return 400;
        }

        // direkt nach ':' muss ein Leerzeichen kommen
        if (*(colon + 1) != ' ') {
            return 400;
        }

        // Value darf nicht leer sein
        if (*(colon + 2) == '\0') {
            return 400;
        }

        // zur nächsten Zeile (über CRLF springen)
        pos += 2;
    }

    // Wenn wir hier sind: alles nach der ersten Zeile ist syntaktisch ok
    return code_from_first_line;  // z.B. 1000, 1001, 1002 oder 501
}

int check_static_resource(const char* uri) {
    if (strcmp(uri, "/static/foo") == 0) {
        return 200; // OK, Body = "Foo"
    }
    if (strcmp(uri, "/static/bar") == 0) {
        return 200; // OK, Body = "Bar"
    }
    if (strcmp(uri, "/static/baz") == 0) {
        return 200; // OK, Body = "Baz"
    }
    return 404;
}

void get_body(const char* buf, int len) {
    // 1. Position des ersten Auftretens von "\r\n\r\n" suchen
    int header_end = find_crlfcrlf(buf, len);

    if (header_end == -1) {
        // Kein Headerende gefunden → Kein Body vorhanden
        body[0] = '\0';
        return;
    }

    // 2. Body beginnt nach "\r\n\r\n"
    int body_start = header_end + 4;

    // 3. Body-Länge berechnen
    int body_len = len - body_start;
    if (body_len < 0) body_len = 0;

    // 4. Überlauf vermeiden
    if (body_len >= (int)sizeof(body)) {
        body_len = (int)sizeof(body) - 1;
    }

    // 5. Body kopieren
    memcpy(body, buf + body_start, body_len);

    // 6. Nullterminieren
    body[body_len] = '\0';
}

int check_dynamic_resource(const char* uri, int code) {

    // Prüft, ob URI mit "/dynamic/" beginnt
    if (strncmp(uri, "/dynamic/", 9) != 0) {
        return 403; // falscher Pfad, forbidden
    } else {

        if (code == 1001) {  // PUT

            // 1. Dateiname extrahieren
            const char *filename = uri + 9; // hinter /dynamic/

            if (filename[0] == '\0') {
                return 404; // kein Filename → ungültig
            }

            // 2. Durch die LinkedList laufen (Resource existiert?)
            DynamicResource *curr = resource_list;
            while (curr != NULL) {

                if (strcmp(curr->filename, filename) == 0) {

                    // RESOURCE EXISTIERT → UPDATE → 204 No Content

                    // alten Inhalt löschen
                    free(curr->content);

                    // neuen Body speichern
                    curr->content = my_strdup(body);

                    return 204;
                }

                curr = curr->next;
            }

            // RESOURCE NICHT GEFUNDEN → CREATED → 201 Created

            DynamicResource *new_node = malloc(sizeof(DynamicResource));
            if (!new_node) return 500; // Out of Memory

            // Filename kopieren
            new_node->filename = my_strdup(filename);

            // Body kopieren
            new_node->content = my_strdup(body);

            // vorne an die Liste hängen
            new_node->next = resource_list;
            resource_list = new_node;

            return 201;
        }

        if (code == 1002) {   // DELETE

            const char *filename = uri + 9;  // konsequent wie beim PUT

            if (filename[0] == '\0') {
                return 404;
            }

            DynamicResource *curr = resource_list;
            DynamicResource *prev = NULL;

            while (curr != NULL) {

                if (strcmp(curr->filename, filename) == 0) {

                    // RESOURCE EXISTIERT → LÖSCHEN → 201 (wie im Originalcode)

                    // Verkettung reparieren
                    if (prev == NULL) {
                        resource_list = curr->next; // erstes Element
                    } else {
                        prev->next = curr->next;
                    }

                    // Speicher freigeben
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
    }

    return 404;
}

int check_Header(const char *buf, int len) {

    // 1. Startzeile prüfen
    int code = check_first_line(buf, len);

    if (code == 400) {
        return 400; // sofort Bad Request
    }

    return check_after_first_line(buf, len, code);
}

int get_code(const char *buf, int len) {

    int code = check_Header(buf, len);

    if (code == 400) {
        return 400;
    }

    if (code == 1000) {
        return check_static_resource(global_uri);
    }

    if (code == 1001) {
        get_body(buf, len);
        return check_dynamic_resource(dynamic_uri, code);
    }

    if (code == 1002) {
        // keinen Body nötig
        return check_dynamic_resource(dynamic_uri, code);
    }

    // 501 oder andere Weitergaben
    return code;
}

// ----------------- Test-Harness -----------------

void run_test(const char *name, const char *req, int expected) {
    int len = (int)strlen(req);
    int code = get_code(req, len);
    printf("%s: expected %d, got %d -> %s\n",
           name, expected, code,
           (code == expected) ? "OK" : "FAIL");
}

int main(void) {

    // 1) Gültiger GET auf bekannte statische Ressource → 200
    const char *req1 =
        "GET /static/foo HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    // 2) Gültiger GET auf unbekannte statische Ressource → 404
    const char *req2 =
        "GET /static/unknown HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    // 3) Ungültige HTTP-Version → 400
    const char *req3 =
        "GET /static/foo HTTP/2.0\r\n"
        "Host: example.com\r\n"
        "\r\n";

    // 4) Ungültiger Header (kein Doppelpunkt) → 400
    const char *req4 =
        "GET /static/foo HTTP/1.1\r\n"
        "Host example.com\r\n"
        "\r\n";

    // 5) PUT auf /dynamic/foo, Ressource noch nicht vorhanden → 201 (Created)
    const char *req5 =
        "PUT /dynamic/foo HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "Hello";

    // 6) PUT auf /dynamic/foo erneut → 204 (No Content, Update)
    const char *req6 =
        "PUT /dynamic/foo HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "World";

    // 7) DELETE auf /dynamic/foo → 201 (wie im Originalcode)
    const char *req7 =
        "DELETE /dynamic/foo HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    // 8) DELETE auf /dynamic/foo erneut → 404 (nicht mehr vorhanden)
    const char *req8 =
        "DELETE /dynamic/foo HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    // 9) PUT mit falschem Pfad (/other/...) → 403 (Forbidden)
    const char *req9 =
        "PUT /other/foo HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "Test";

    // 10) Nicht unterstützte Methode (POST) → 501
    const char *req10 =
        "POST /static/foo HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n";

    run_test("Test 1  (GET /static/foo -> 200)",         req1, 200);
    run_test("Test 2  (GET /static/unknown -> 404)",     req2, 404);
    run_test("Test 3  (HTTP-Version falsch -> 400)",     req3, 400);
    run_test("Test 4  (Header ohne Doppelpunkt -> 400)", req4, 400);
    run_test("Test 5  (PUT /dynamic/foo -> 201)",        req5, 201);
    run_test("Test 6  (PUT /dynamic/foo -> 204)",        req6, 204);
    run_test("Test 7  (DELETE /dynamic/foo -> 201)",     req7, 201);
    run_test("Test 8  (DELETE /dynamic/foo -> 404)",     req8, 404);
    run_test("Test 9  (PUT falscher Pfad -> 403)",       req9, 403);
    run_test("Test 10 (POST -> 501)",                    req10, 501);

    return 0;
}