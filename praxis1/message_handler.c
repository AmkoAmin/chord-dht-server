#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>

//global
char global_uri[256];

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
        strcpy(global_uri, uri);
        return 1000; // gültiger GET-Request
    } else {
        return 501; // gültig, aber nicht GET
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

    if (code == 1000)
    {
        return check_static_resource(global_uri);
    }

    // -------------------------------------------------------------
    // TODO: Check Body (Content-Length etc.)
    // in Aufgabe 2.5 noch NICHT benötigt
    // -------------------------------------------------------------

    return code;
}


void get_messages_and_send(int sockfd, char *buf, int *buf_len) {
    
    const char reply_Bad_Request[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    const char reply_Not_Found[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
    const char reply_Not_Implemented[] = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
    const char reply_OK_prefix[] = "HTTP/1.1 200 OK\r\nContent-Length: ";

    while (true) {
        
        const char* reply = NULL;

        int end = find_crlfcrlf(buf, *buf_len);
        
        if (end == -1) {
            // Kein komplettes Paket ("\\r\\n\\r\\n") mehr im Buffer → fertig
            break;
        }

        // Position von "\r\n\r\n" ist 'end'
        int msg_len = end + 4; // +4 Zeichen für "\r\n\r\n"

        int code = get_code(buf, msg_len);

        if (code == 400){
            
            reply = reply_Bad_Request;

        } else if (code == 404){
            
            reply = reply_Not_Found;

        } else if (code == 501){
            
            reply = reply_Not_Implemented;

        } else if (code == 200){
            const char *body = "";
            if (strcmp(global_uri, "/static/foo") == 0) body = "Foo";
            else if (strcmp(global_uri, "/static/bar") == 0) body = "Bar";
            else if (strcmp(global_uri, "/static/baz") == 0) body = "Baz";

            int body_len = strlen(body);

            static char reply_buf[512];

            int written = snprintf(
                reply_buf,
                sizeof(reply_buf),
                "%s%d\r\n\r\n%s",
                reply_OK_prefix,   // Prefix ENTHALTEN
                body_len,
                body
            );

            if (written == -1){
                perror("Fehler");
                return;
            }

            reply = reply_buf;

        } else {
            reply = reply_Not_Implemented;
        }

        // 2. Antwort "Reply\r\n\r\n" an denselben Socket senden:
        int sent = send(sockfd, reply, strlen(reply), 0);
        if (sent == -1) {
            perror("send");
            break;
        }

        // 3. Verarbeitetes Paket aus dem Buffer entfernen:
        int remaining = *buf_len - msg_len;

        if (remaining > 0) {
            // Rest an den Anfang schieben:
            memmove(buf, buf + msg_len, remaining);
        }

        // Neue Länge des Buffers:
        *buf_len = remaining;
    }
}