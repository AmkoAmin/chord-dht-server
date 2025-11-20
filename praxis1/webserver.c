#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <unistd.h>

/*
 * Ab hier Amins Header
 */

#include "message_handler.h"

int main(int argc, char *argv[])
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size;

    struct addrinfo hints, *res;
    int sockfd, new_fd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     

    getaddrinfo(argv[1], argv[2], &hints, &res);

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    printf("SocketFD: %d\n", sockfd);

    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

    int bind_info = bind(sockfd, res->ai_addr, res->ai_addrlen);
    printf("Bind-Info: %d\n", bind_info);
    
    int lausch_info = listen(sockfd, 100);

    printf("Lausch-Info: %d\n", lausch_info);

    while (true)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        
        char buff[8192];
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
            
            get_messages_and_send(new_fd, buff, &buf_len);
        }
        close(new_fd);
    }

    freeaddrinfo(res);
    return 0;
}