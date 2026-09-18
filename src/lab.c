#include "lab.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

int open_close(unsigned short port, char* hostname, char* message){
  int sockfd = socket(AF_INET, SOCK_STREAM, 0); //file descriptor
    if (sockfd < 0) return 1;

    struct hostent * server_hostent = gethostbyname(hostname);
    in_addr_t serverIPV4 = *(in_addr_t *)server_hostent->h_addr_list[0];
    
    struct sockaddr_in server_addr = {
      .sin_family = AF_INET, // Address family
      .sin_port = htons(port), // Port number (converted to network byte order)
      .sin_addr.s_addr = serverIPV4, // IP address
    };
    
    int res = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (res < 0) {
      return 1;
    }
    
    char reply[6000];
    res = recv(sockfd, reply, 6000, 0);
    if (res < 0){
      return 1;
    }
    printf("%s\n", reply);
    

    struct in_addr *server_in = (struct in_addr *)server_hostent->h_addr_list[0];
    char *server_ipv4 = inet_ntoa(*server_in);
    

    
    send(sockfd, message, strlen(message), 0);
    

    close(sockfd);
}