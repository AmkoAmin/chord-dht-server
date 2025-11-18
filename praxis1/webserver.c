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

int main(char *argv[])
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size;

    struct addrinfo hints, *res;
    int sockfd, new_fd;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    getaddrinfo(argv[1], argv[2], &hints, &res);

    // make a socket:

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    printf("SocketFD: %d\n", sockfd);

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    int bind_info = bind(sockfd, res->ai_addr, res->ai_addrlen);
    printf("Bind-Info: %d\n", bind_info);
    
    int lausch_info = listen(sockfd, 10);

    printf("Lausch-Info: %d\n", lausch_info);

    while (true)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        
        char buff[4096];
        int buf_len = 0;

        while (true)
        {  
            int bytes = recv(new_fd, buff + buf_len, sizeof(buff) - buf_len, 0);

            if (bytes <= 0)
            {
                printf("Communcation beendet!");
                break;
            }
            
            buf_len += bytes;
            
            get_messages(new_fd, buff, &buf_len);
        }
        close(new_fd);
    }

    freeaddrinfo(res);
    return 0;
}