#ifndef LAB_H
#define LAB_H

int smtp_protcol(char* from, char* to, char* subject, char* body, unsigned short port, char* helo_host, char* host_name_addr);
int read_sock(int sockfd, char* reply);

#endif // LAB_H
