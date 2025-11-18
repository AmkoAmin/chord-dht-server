#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>

int find_crlfcrlf(const char *buf, int len) {
    
    for (int i = 0; i <= len - 4; i++) {
        
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            return i;
        }

    }

    return -1;
}

void get_messages_and_send(int sockfd, char *buf, int *buf_len) {
    
    const char reply[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";

    while (true) {
        
        int end = find_crlfcrlf(buf, *buf_len);
        
        if (end == -1) {
            // Kein komplettes Paket ("\\r\\n\\r\\n") mehr im Buffer → fertig
            break;
        }

        // Position von "\r\n\r\n" ist 'end'
        int msg_len = end + 4; // +4 Zeichen für "\r\n\r\n"

        // 2. Antwort "Reply\r\n\r\n" an denselben Socket senden:
        size_t sent = send(sockfd, reply, sizeof(reply) - 1, 0);
        if (sent == -1) {
            perror("send");
            // Bei Fehler brechen wir ab – Buffer behalten wir so wie er ist
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