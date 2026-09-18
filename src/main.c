#include "lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

#define DEFAULT_PORT "25"
#define DEFAULT_HELO "localhost"
#define DEFAULT_SUBJECT ""

void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s -f <from> -t <to> [-s subject] [-b body] [-p port]\n", progname);
    fprintf(stderr, "          [-H helo-host] <server>\n");
    fprintf(stderr, "  -f <from>       envelope sender, for example you@example.com\n");
    fprintf(stderr, "  -t <to>         envelope recipient\n");
    fprintf(stderr, "  -s <subject>    subject line (default: empty)\n");
    fprintf(stderr, "  -b <body>       message body (default: read from stdin)\n");
    fprintf(stderr, "  -p <port>       port or service name (default: 25)\n");
    fprintf(stderr, "  -H <helo-host>  host name sent with HELO (default: localhost)\n");
    exit(EXIT_FAILURE);
}

char *read_stdin_to_string(void) {
    size_t size = 1024;
    size_t len = 0;
    char *buffer = malloc(size);
    if (!buffer) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    int c;
    while ((c = getchar()) != EOF) {
        if (len + 1 >= size) {
            size *= 2;
            char *new_buffer = realloc(buffer, size);
            if (!new_buffer) {
                perror("realloc failed");
                free(buffer);
                exit(EXIT_FAILURE);
            }
            buffer = new_buffer;
        }
        buffer[len++] = (char)c;
    }
    buffer[len] = '\0';
    return buffer;
}

int main(int argc, char *argv[]) {
    char *from = NULL;
    char *to = NULL;
    char *subject = DEFAULT_SUBJECT;
    char *body = NULL;
    char *port = DEFAULT_PORT;
    char *helo_host = DEFAULT_HELO;
    char *server = NULL;
    
    // Flag to track dynamic memory allocation
    int is_body_allocated = 0;

    int opt;
    while ((opt = getopt(argc, argv, "f:t:s:b:p:H:")) != -1) {
        switch (opt) {
            case 'f': from = optarg; break;
            case 't': to = optarg; break;
            case 's': subject = optarg; break;
            case 'b': body = optarg; break;
            case 'p': port = optarg; break;
            case 'H': helo_host = optarg; break;
            default: print_usage(argv[0]);
        }
    }

    if (!from || !to) {
        fprintf(stderr, "Error: Both -f <from> and -t <to> are required.\n\n");
        print_usage(argv[0]);
        return 0;
    }

    if (optind < argc) {
        server = argv[optind];
    } else {
        fprintf(stderr, "Error: Missing required <server> argument.\n\n");
        print_usage(argv[0]);
        return 0;
    }

    // If body (-b) was not provided, read from stdin and set allocation flag
    if (!body) {
        body = read_stdin_to_string();
        is_body_allocated = 1;
    }

    printf("--- Configuration Parsed Successfully ---\n");
    printf("Server:    %s\n", server);
    printf("Port:      %s\n", port);
    printf("From:      %s\n", from);
    printf("To:        %s\n", to);
    printf("HELO Host: %s\n", helo_host);
    printf("Subject:   %s\n", subject);
    printf("Body:      %s\n\n", body);

    printf("--- SMTP Protocol Status ---\n\n");
    int res = smtp_protcol(from, to, subject, body, (short unsigned int)atoi(port), helo_host, server);

    printf("--- SMTP Protocol Executed Successfully ---\n\n");
    
    // Memory Cleanup: Free body if it was dynamically allocated
    if (is_body_allocated) {
        free(body);
    }
    
    res == 1 ? res++ : res;
    return res;
}