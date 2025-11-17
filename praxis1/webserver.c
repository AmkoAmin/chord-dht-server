#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>

int main(int argc, char *argv[])
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

    // bind it to the port we passed in to getaddrinfo():
    int bind_info = bind(sockfd, res->ai_addr, res->ai_addrlen);
    printf("Bind-Info: %d\n", bind_info);
    
    int lausch_info = listen(sockfd, 10);

    printf("Lausch-Info: %d\n", lausch_info);

    while (true)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        
        char buff[1024];
        int bytes = recv(new_fd, buff, sizeof(buff), 0);

        printf("Bytes angekommen: %d\n", bytes);

        char *msg = "Reply";
        
        int bytes_sent = send(new_fd, msg, strlen(msg), 0);
    }

    freeaddrinfo(res);
    return 0;
}