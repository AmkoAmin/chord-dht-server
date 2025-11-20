#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>

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
        return 404; // gültiger GET-Request
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

int check_Header(const char *buf, int len) {

    // 1. Startzeile prüfen
    int code = check_first_line(buf, len);

    if (code == 400) {
        return 400; // sofort Bad Request
    }

    return check_after_first_line(buf, len, code);
}

void get_messages(int sockfd, char *buf, int *buf_len) {
    
    const char reply[] = "Reply\r\n\r\n";

    while (true) {
        
        int end = find_crlfcrlf(buf, *buf_len);
        
        if (end == -1) {
            // Kein komplettes Paket ("\\r\\n\\r\\n") mehr im Buffer → fertig
            break;
        }

        // Position von "\r\n\r\n" ist 'end'
        int msg_len = end + 4; // +4 Zeichen für "\r\n\r\n"

        // 2. Antwort "Reply\r\n\r\n" an denselben Socket senden:
        printf("gesendet");

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

void run_test(const char *name, const char *req, int expected) {
    int len = (int)strlen(req);
    int code = check_Header(req, len);
    printf("%s: expected %d, got %d -> %s\n",
           name, expected, code,
           (code == expected) ? "OK" : "FAIL");
}

int main(void) {
    // 1) Gültiger GET + gültige Header → 404
    const char *req1 =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: TestClient\r\n"
        "\r\n";

    // 2) Gültiger POST + gültige Header → 501
    const char *req2 =
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n";

    // 3) Ungültige Startzeile (falsche HTTP-Version) → 400
    const char *req3 =
        "GET / HTTP/2.0\r\n"
        "Host: example.com\r\n"
        "\r\n";

    // 4) Ungültiger Header (kein Doppelpunkt) → 400
    const char *req4 =
        "GET / HTTP/1.1\r\n"
        "Host example.com\r\n"   // ❌ fehlt das ':'
        "\r\n";

    // 5) Ungültiger Header (Doppelpunkt, aber kein Leerzeichen/Wert) → 400
    const char *req5 =
        "GET / HTTP/1.1\r\n"
        "Host:\r\n"              // ❌ kein "Key: Value"
        "\r\n";

    run_test("Test 1 (GET -> 404)",              req1, 404);
    run_test("Test 2 (POST -> 501)",             req2, 501);
    run_test("Test 3 (HTTP-Version falsch)",     req3, 400);
    run_test("Test 4 (Header ohne Doppelpunkt)", req4, 400);
    run_test("Test 5 (Header ohne Wert)",        req5, 400);

    return 0;
}
