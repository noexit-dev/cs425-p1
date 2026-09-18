#include "harness/unity.h"
#include "../src/lab.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>


// Mock server scenario configurations
typedef enum {
    SCENARIO_HAPPY,
    SCENARIO_MULTILINE,
    SCENARIO_FRAGMENTED,
    SCENARIO_BUFFER_OVERFLOW,
    SCENARIO_HANGUP,
    SCENARIO_WRONG_GREETING,
    SCENARIO_WRONG_HELO,
    SCENARIO_WRONG_MAILFROM,
    SCENARIO_WRONG_RCPTTO,
    SCENARIO_WRONG_DATA,
    SCENARIO_WRONG_QUIT
} test_scenario_t;

static test_scenario_t current_scenario = SCENARIO_HAPPY;
static int mock_server_port = 12525;
static pthread_t server_thread;
static int server_running = 0;



void setUp(void) {
  printf("Setting up tests...\n");
}

void tearDown(void) {
  printf("Tearing down tests...\n");
}

// --- Mock SMTP Server Thread ---
void* mock_smtp_server(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(mock_server_port)
    };

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 3) < 0) {
        close(server_fd);
        return NULL;
    }

    server_running = 1;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        close(server_fd);
        return NULL;
    }

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));

    if (current_scenario == SCENARIO_HANGUP) {
        close(client_fd);
        close(server_fd);
        return NULL;
    }

    // 1. Greeting
    if (current_scenario == SCENARIO_WRONG_GREETING) {
        write(client_fd, "500 Service unavailable\r\n", 25);
    } else if (current_scenario == SCENARIO_MULTILINE) {
        write(client_fd, "220-Line 1\r\n220 Line 2 ready\r\n", 28);
    } else if (current_scenario == SCENARIO_FRAGMENTED) {
        write(client_fd, "2", 1);
        usleep(5000);
        write(client_fd, "20", 2);
        usleep(5000);
        write(client_fd, " Ready\r\n", 8);
    } else if (current_scenario == SCENARIO_BUFFER_OVERFLOW) {
        char huge_reply[7000];
        memset(huge_reply, '2', sizeof(huge_reply));
        memcpy(huge_reply, "220 ", 4);
        memcpy(huge_reply + 6998, "\r\n", 2);
        write(client_fd, huge_reply, sizeof(huge_reply));
    } else {
        write(client_fd, "220 localhost ESMTP\r\n", 21);
    }

    // 2. HELO
    read(client_fd, buffer, sizeof(buffer));
    if (current_scenario == SCENARIO_WRONG_HELO) {
        write(client_fd, "504 Command not implemented\r\n", 29);
        close(client_fd);
        close(server_fd);
        return NULL;
    }
    write(client_fd, "250 Hello\r\n", 11);

    // 3. MAIL FROM
    read(client_fd, buffer, sizeof(buffer));
    if (current_scenario == SCENARIO_WRONG_MAILFROM) {
        write(client_fd, "550 Access denied\r\n", 19);
        close(client_fd);
        close(server_fd);
        return NULL;
    }
    write(client_fd, "250 OK\r\n", 8);

    // 4. RCPT TO
    read(client_fd, buffer, sizeof(buffer));
    if (current_scenario == SCENARIO_WRONG_RCPTTO) {
        write(client_fd, "550 No such user\r\n", 18);
        close(client_fd);
        close(server_fd);
        return NULL;
    }
    write(client_fd, "250 OK\r\n", 8);

    // 5. DATA
    read(client_fd, buffer, sizeof(buffer));
    if (current_scenario == SCENARIO_WRONG_DATA) {
        write(client_fd, "554 Transaction failed\r\n", 24);
        close(client_fd);
        close(server_fd);
        return NULL;
    }
    write(client_fd, "354 Start mail input\r\n", 22);

    // 6. Message Body + End Marker (.)
    read(client_fd, buffer, sizeof(buffer));
    write(client_fd, "250 OK queued\r\n", 15);

    // 7. QUIT
    read(client_fd, buffer, sizeof(buffer));
    if (current_scenario == SCENARIO_WRONG_QUIT) {
        write(client_fd, "500 Error\r\n", 11);
    } else {
        write(client_fd, "221 Bye\r\n", 9);
    }

    close(client_fd);
    close(server_fd);
    return NULL;
}

void start_mock_server(void) {
    server_running = 0;
    pthread_create(&server_thread, NULL, mock_smtp_server, NULL);
    while (!server_running) { usleep(5000); }
}

void stop_mock_server(void) {
    pthread_join(server_thread, NULL);
}

// --- Direct Tests for read_sock ---
void test_read_sock_success(void) {
    int sv[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    write(sv[0], "250 Success\r\n", 13);

    char reply[6000];
    int res = read_sock(sv[1], reply);
    TEST_ASSERT_EQUAL(0, res);
    TEST_ASSERT_EQUAL_STRING("250 Success\r\n", reply);

    close(sv[0]);
    close(sv[1]);
}

void test_read_sock_failure_status(void) {
    int sv[2];
    TEST_ASSERT_EQUAL(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
    write(sv[0], "500 Error\r\n", 11);

    char reply[6000];
    int res = read_sock(sv[1], reply);
    TEST_ASSERT_EQUAL(1, res);

    close(sv[0]);
    close(sv[1]);
}

// --- Protocol Tests for smtp_protcol ---
void test_smtp_protocol_happy(void) {
    current_scenario = SCENARIO_HAPPY;
    start_mock_server();
    int res = smtp_protcol("from@test.com", "to@test.com", "Sub", "Body", mock_server_port, "host", "127.0.0.1");
    TEST_ASSERT_EQUAL(0, res);
    stop_mock_server();
}

void test_smtp_protocol_multiline(void) {
    current_scenario = SCENARIO_MULTILINE;
    start_mock_server();
    int res = smtp_protcol("from@test.com", "to@test.com", "Sub", "Body", mock_server_port, "host", "127.0.0.1");
    TEST_ASSERT_EQUAL(0, res);
    stop_mock_server();
}

void test_smtp_protocol_fragmented(void) {
    current_scenario = SCENARIO_FRAGMENTED;
    start_mock_server();
    int res = smtp_protcol("from@test.com", "to@test.com", "Sub", "Body", mock_server_port, "host", "127.0.0.1");
    TEST_ASSERT_EQUAL(0, res);
    stop_mock_server();
}

void test_smtp_protocol_buffer_overflow(void) {
    current_scenario = SCENARIO_BUFFER_OVERFLOW;
    start_mock_server();
    int res = smtp_protcol("from@test.com", "to@test.com", "Sub", "Body", mock_server_port, "host", "127.0.0.1");
    // Should handle gracefully based on read limits
    (void)res;
    stop_mock_server();
}

void test_smtp_protocol_hangup(void) {
    current_scenario = SCENARIO_HANGUP;
    start_mock_server();
    int res = smtp_protcol("from@test.com", "to@test.com", "Sub", "Body", mock_server_port, "host", "127.0.0.1");
    TEST_ASSERT_NOT_EQUAL(0, res);
    stop_mock_server();
}

void test_smtp_protocol_wrong_codes(void) {
    test_scenario_t scenarios[] = {
        SCENARIO_WRONG_GREETING,
        SCENARIO_WRONG_HELO,
        SCENARIO_WRONG_MAILFROM,
        SCENARIO_WRONG_RCPTTO,
        SCENARIO_WRONG_DATA,
        SCENARIO_WRONG_QUIT
    };

    for (size_t i = 0; i < sizeof(scenarios)/sizeof(scenarios[0]); i++) {
        current_scenario = scenarios[i];
        start_mock_server();
        int res = smtp_protcol("from@test.com", "to@test.com", "Sub", "Body", mock_server_port, "host", "127.0.0.1");
        TEST_ASSERT_NOT_EQUAL(0, res);
        stop_mock_server();
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_sock_success);
    RUN_TEST(test_read_sock_failure_status);
    RUN_TEST(test_smtp_protocol_happy);
    RUN_TEST(test_smtp_protocol_multiline);
    RUN_TEST(test_smtp_protocol_fragmented);
    RUN_TEST(test_smtp_protocol_buffer_overflow);
    RUN_TEST(test_smtp_protocol_hangup);
    RUN_TEST(test_smtp_protocol_wrong_codes);
    return UNITY_END();
}