#include "lab.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

int read_sock(int sockfd, char* reply){
  long int res = read(sockfd, reply, 6000);
  
  if (res < 0){
    printf("Error reading from socket\n");
    return 1;
  }
  
  reply[res] = '\0';
  printf("%s", reply);  // Print all responses
  
  // Accept 2xx and 3xx response codes (success and waiting for data)
  if ((strncmp(reply, "2", 1) == 0 || strncmp(reply, "3", 1) == 0)) {
    return 0;
  } else {
    printf("Error: unexpected response code\n");
    return 1;
  }
}

int smtp_protcol(char* from, char* to, char* subject, char* body, unsigned short port, char* helo_host, char* host_name_addr){
  char newline[] = "\r\n";

  int sockfd = socket(AF_INET, SOCK_STREAM, 0); //file descriptor
  if (sockfd < 0) return 1;

  struct hostent * server_hostent = gethostbyname(host_name_addr);
  if (server_hostent == NULL) return 1;
  
  in_addr_t serverIPV4 = *(in_addr_t *)server_hostent->h_addr_list[0];
  
  struct sockaddr_in server_addr = {
    .sin_family = AF_INET, // Address family
    .sin_port = htons(port), // Port number (converted to network byte order)
    .sin_addr.s_addr = serverIPV4, // IP address
  };
  
  long int res = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (res < 0) {
    return 1;
  }
  
  char reply[6000];
  
  // Read initial server greeting
  memset(reply, 0, sizeof(reply));
  if (read_sock(sockfd, reply) != 0) return 1;

  // Send HELO command
  char helo[512];
  strcpy(helo, "HELO ");
  strcat(helo, helo_host);
  strcat(helo, newline);
  
  res = send(sockfd, helo, strlen(helo), 0);
  if (res == -1) return 1;
  
  memset(reply, 0, sizeof(reply));
  if (read_sock(sockfd, reply) != 0) return 1;
  
  // Send MAIL FROM command
  char mailfrom[512];
  strcpy(mailfrom, "MAIL FROM:<");
  strcat(mailfrom, from);
  strcat(mailfrom, ">");
  strcat(mailfrom, newline);
  
  res = send(sockfd, mailfrom, strlen(mailfrom), 0);
  if (res == -1) return 1;
  
  memset(reply, 0, sizeof(reply));
  if (read_sock(sockfd, reply) != 0) return 1;
  printf("%s\n", reply);
  // Send RCPT TO command
  char rcptto[512];
  strcpy(rcptto, "RCPT TO:<");
  strcat(rcptto, to);
  strcat(rcptto, ">");
  strcat(rcptto, newline);
  
  res = send(sockfd, rcptto, strlen(rcptto), 0);
  if (res == -1) return 1;
  
  memset(reply, 0, sizeof(reply));
  if (read_sock(sockfd, reply) != 0) return 1;
  printf("%s\n", reply);
  // Send DATA command
  char data[512];
  strcpy(data, "DATA");
  strcat(data, newline);
  
  res = send(sockfd, data, strlen(data), 0);
  if (res == -1) return 1;
  
  memset(reply, 0, sizeof(reply));
  if (read_sock(sockfd, reply) != 0) return 1;
  printf("%s\n", reply);
  
  char message[4096];
  strcpy(message, "Subject: ");
  strcat(message, subject);
  strcat(message, newline);
  strcat(message, "From: ");
  strcat(message, from);
  strcat(message, newline);
  strcat(message, "To: ");
  strcat(message, to);
  strcat(message, newline);
  strcat(message, newline);
  strcat(message, body);
  strcat(message, newline);
  
  res = send(sockfd, message, strlen(message), 0);
  if (res == -1) return 1;
  
  // Send end-of-data marker
  char stop[512];
  strcpy(stop, ".");
  strcat(stop, newline);
  
  res = send(sockfd, stop, strlen(stop), 0);
  if (res == -1) return 1;
  
  memset(reply, 0, sizeof(reply));
  if (read_sock(sockfd, reply) != 0) return 1;
  printf("%s\n", reply);
  
  char quit[512];
  strcpy(quit, "QUIT");
  strcat(quit, newline);
  
  res = send(sockfd, quit, strlen(quit), 0);
  if (res == -1) return 1;
  
  memset(reply, 0, sizeof(reply));
  if (read_sock(sockfd, reply) != 0) return 1;
  printf("%s\n", reply);
  
  return 0; //success
}