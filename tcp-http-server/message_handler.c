#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>

//global
char request_uri[256];
char body[8192];

typedef struct DynamicResource {
    char* filename;          // z. B. "foo", "bar.json", ...
    char* content;           // Inhalt der Datei/Ressource
    struct DynamicResource *next;   // Zeiger auf das nächste Element
} DynamicResource;

DynamicResource* resource_list = NULL;

static char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

int find_crlfcrlf(const char *buf, int len) {
    
    for (int i = 0; i <= len - 4; i++) {
        
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
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

        int line_len = pos - line_start;

        // Leerzeile? → Header zu Ende → fertig
        if (line_len == 0) {
            break;
        }

        // Headerzeile in temporären Buffer kopieren
        char hline[512];
        if (line_len >= 511) {
            return 400; // Header viel zu lang
        }
        memcpy(hline, buf + line_start, line_len);
        hline[line_len] = '\0';

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
        strcpy(request_uri, uri);
        return 1000; // gültiger GET-Request
    } else if(strcmp(method, "PUT") == 0){
        strcpy(request_uri, uri);
        return 1001;
    } else if(strcmp(method, "DELETE") == 0){
        strcpy(request_uri, uri);
        return 1002;
    }
    else {
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
        }    return code_from_first_line;  // 404 oder 501


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
    return code_from_first_line;  // 404 oder 501
}

int check_static_resource(const char* uri){
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

int get_dynamic_ressource(const char* uri) {
    // Erwartetes Prefix prüfen
    if (strncmp(uri, "/dynamic/", 9) != 0) {
        return 404;  // oder 403, je nach gewünschter Semantik
    }

    const char *filename = uri + 9; // Teil hinter "/dynamic/"

    if (filename[0] == '\0') {
        // Kein Dateiname
        return 404;
    }

    DynamicResource *curr = resource_list;

    while (curr != NULL) {
        if (strcmp(curr->filename, filename) == 0) {
            // Gefunden -> Inhalt in globales Array kopieren
            
            // DEBUG: prüfen, ob Inhalt vorhanden ist
            printf("DEBUG: GET found resource '%s' with content='%s' (len=%zu)\n",
               curr->filename,
               curr->content ? curr->content : "(null)",
               curr->content ? strlen(curr->content) : 0
            );

            size_t len = strlen(curr->content);
            if (len >= sizeof(body)) {
                len = sizeof(body) - 1;  // Überlauf verhindern
            }

            memcpy(body, curr->content, len);
            body[len] = '\0';

            return 200;
        }
        curr = curr->next;
    }

    // Nicht gefunden
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
    if (body_len >= sizeof(body)) {
        body_len = sizeof(body) - 1;
    }

    // 5. Body kopieren
    memcpy(body, buf + body_start, body_len);

    // 6. Nullterminieren
    body[body_len] = '\0';
}

int check_dynamic_resource(const char* uri, int code){
    
    // Prüft, ob URI mit "/static/" beginnt
    if (strncmp(uri, "/dynamic/", 9) != 0) {
        return 403; // falscher Pfad, forbidden
    } else {
        if (code == 1001)
        {
            // 1. Dateiname extrahieren
            const char *filename = uri + 9; // hinter /static/

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

            // -----------------------------------------------------
            // TODO erledigt: Neue Resource an Liste anhängen
            // -----------------------------------------------------

            // RESOURCE NICHT GEFUNDEN → CREATED → 201 Created

            DynamicResource *new_node = malloc(sizeof(DynamicResource));
            if (!new_node) return 500; // optional: Out of Memory

            // Filename kopieren
            new_node->filename = my_strdup(filename);

            // Body kopieren
            new_node->content = my_strdup(body);

            // vorne an die Liste hängen
            new_node->next = resource_list;
            resource_list = new_node;

            return 201;  

        }

        if (code == 1002)
    {
        const char *filename = uri + 9;

        if (filename[0] == '\0') {
            return 404;
        }

        DynamicResource *curr = resource_list;
        DynamicResource *prev = NULL;

        while (curr != NULL) {

            if (strcmp(curr->filename, filename) == 0) {

                // RESOURCE EXISTIERT → LÖSCHEN → 201 (wie gewünscht)

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

                return 204;
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

    if (code == 1000){
        if (strncmp(request_uri, "/static/", 8) == 0) {
            return check_static_resource(request_uri);
        } else if (strncmp(request_uri, "/dynamic/", 9) == 0) {
            return get_dynamic_ressource(request_uri); 
        }else{
            return 404; //??
        }
    }

    if (code == 1001)
    {
        get_body(buf, len);
        return check_dynamic_resource(request_uri, code);
    }

    if (code == 1002)
    {
        return check_dynamic_resource(request_uri, code);
    }

    return code;
}

void get_messages_and_send(int sockfd, char *buf, int *buf_len) {
    
    const char reply_Bad_Request[]   = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    const char reply_Not_Found[]     = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    const char reply_Not_Implemented[]= "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
    const char reply_OK_prefix[]     = "HTTP/1.1 200 OK\r\nContent-Length: ";
    const char reply_forbidden[]     = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
    const char reply_created[]       = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
    const char reply_no_content[]    = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n";

    while (true) {

        const char *reply = NULL;

        // Haben wir überhaupt genug Daten für ein Header-Ende?
        if (*buf_len < 4) {
            break;
        }

        int header_end = find_crlfcrlf(buf, *buf_len);
        if (header_end == -1) {
            // Header noch nicht komplett im Buffer
            break;
        }

        int header_len = header_end + 4;  // inkl. "\r\n\r\n"

        // -------------------------------
        // Content-Length aus den Headern lesen (falls vorhanden)
        // -------------------------------
        int content_length = 0;

        // Header in temporären Buffer kopieren und terminieren
        char header_block[4096];
        int copy_len = header_len < (int)sizeof(header_block) - 1
                       ? header_len
                       : (int)sizeof(header_block) - 1;
        memcpy(header_block, buf, copy_len);
        header_block[copy_len] = '\0';

        // Nach "Content-Length:" suchen (Case-sensitive reicht hier)
        char *cl = strstr(header_block, "Content-Length:");
        if (cl) {
            cl += strlen("Content-Length:");
            // Whitespace überspringen
            while (*cl == ' ' || *cl == '\t') {
                cl++;
            }
            content_length = atoi(cl);
            if (content_length < 0) {
                content_length = 0;
            }
        }

        int total_len = header_len + content_length;

        // Haben wir schon den kompletten Body im Buffer?
        if (*buf_len < total_len) {
            // Noch nicht alles da → auf mehr recv warten
            break;
        }

        // Jetzt haben wir eine komplette HTTP-Nachricht: [Header][Body]
        int code = get_code(buf, total_len);

        if (code == 400) {
            reply = reply_Bad_Request;
        } else if (code == 404) {
            reply = reply_Not_Found;
        } else if (code == 501) {
            reply = reply_Not_Implemented;
        } else if (code == 403) {
            reply = reply_forbidden;
        } else if (code == 201) {
            reply = reply_created;
        } else if (code == 204) {
            reply = reply_no_content;
        } else if (code == 200) {

            const char *resp_body = "";

            if (strncmp(request_uri, "/dynamic/", 9) == 0) {
                // Dynamische Ressource: get_dynamic_ressource hat body[] gefüllt
                resp_body = body;
            } else {
                // Statische Ressourcen
                if      (strcmp(request_uri, "/static/foo") == 0) resp_body = "Foo";
                else if (strcmp(request_uri, "/static/bar") == 0) resp_body = "Bar";
                else if (strcmp(request_uri, "/static/baz") == 0) resp_body = "Baz";
                else                                              resp_body = "";
            }

            int body_len = (int)strlen(resp_body);
            static char reply_buf[9000];

            int written = snprintf(
                reply_buf,
                sizeof(reply_buf),
                "%s%d\r\n\r\n%s",
                reply_OK_prefix,
                body_len,
                resp_body
            );

            if (written < 0 || written >= (int)sizeof(reply_buf)) {
                perror("snprintf");
                return;
            }

            reply = reply_buf;

        } else {
            reply = reply_Not_Implemented;
        }

        // Antwort senden
        int sent = send(sockfd, reply, strlen(reply), 0);
        if (sent == -1) {
            perror("send");
            break;
        }

        // Verarbeitete Nachricht aus dem Buffer entfernen:
        int remaining = *buf_len - total_len;
        if (remaining > 0) {
            memmove(buf, buf + total_len, remaining);
        }
        *buf_len = remaining;
    }
}
