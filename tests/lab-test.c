#include "harness/unity.h"
#include "../src/lab.h"
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/syscall.h>

/* --- Mock Global State --- */
int mock_socket_ret;
struct hostent* mock_gethostbyname_ret;
struct hostent dummy_hostent;
char* dummy_addr_list[2];
in_addr_t dummy_addr_val;

int mock_connect_ret;

int send_call_count;
int fail_send_at_call;

int read_call_count;
int fail_read_at_call;
char read_mock_data[256];

/* --- System Call Interceptors (Mocks) --- */
int socket(int domain, int type, int protocol) {
    (void)domain; (void)type; (void)protocol;
    return mock_socket_ret;
}

struct hostent *gethostbyname(const char *name) {
    (void)name;
    return mock_gethostbyname_ret;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)sockfd; (void)addr; (void)addrlen;
    return mock_connect_ret;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    (void)sockfd; (void)buf; (void)flags;
    if (sockfd == 999) {
        if (send_call_count == fail_send_at_call) {
            send_call_count++;
            return -1;
        }
        send_call_count++;
        return (ssize_t)len;
    }
    return -1;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (fd == 999) {
        if (read_call_count == fail_read_at_call) {
            read_call_count++;
            return -1;
        }
        read_call_count++;
        size_t len = strlen(read_mock_data);
        if (count < len) len = count;
        memcpy(buf, read_mock_data, len);
        return (ssize_t)len;
    }
    /* Fallback to actual system call for standard test runner I/O */
    return syscall(SYS_read, fd, buf, count);
}

/* --- Unity Setup and Teardown --- */
void setUp(void) {
    dummy_addr_val = inet_addr("127.0.0.1");
    dummy_addr_list[0] = (char*)&dummy_addr_val;
    dummy_addr_list[1] = NULL;
    dummy_hostent.h_addr_list = dummy_addr_list;

    mock_socket_ret = 999; /* Use isolated file descriptor for mocks */
    mock_gethostbyname_ret = &dummy_hostent;
    mock_connect_ret = 0;

    send_call_count = 0;
    fail_send_at_call = -1; /* -1 means never fail */

    read_call_count = 0;
    fail_read_at_call = -1;
    strcpy(read_mock_data, "250 OK\r\n");
}

void tearDown(void) {
}

/* --- read_sock() Branch Tests --- */
void test_read_sock_success_2xx(void) {
    char reply[6000];
    strcpy(read_mock_data, "250 OK\r\n");
    int res = read_sock(999, reply);
    TEST_ASSERT_EQUAL(0, res);
}

void test_read_sock_success_3xx(void) {
    char reply[6000];
    strcpy(read_mock_data, "354 Start mail input\r\n");
    int res = read_sock(999, reply);
    TEST_ASSERT_EQUAL(0, res);
}

void test_read_sock_fail_unexpected(void) {
    char reply[6000];
    strcpy(read_mock_data, "500 Error\r\n");
    int res = read_sock(999, reply);
    TEST_ASSERT_EQUAL(1, res);
}

void test_read_sock_fail_read(void) {
    char reply[6000];
    fail_read_at_call = 0;
    int res = read_sock(999, reply);
    TEST_ASSERT_EQUAL(1, res);
}

/* --- smtp_protcol() Execution Path Tests --- */
void test_smtp_socket_fail(void) {
    mock_socket_ret = -1;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_gethostbyname_fail(void) {
    mock_gethostbyname_ret = NULL;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_connect_fail(void) {
    mock_connect_ret = -1;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_read_greeting_fail(void) {
    fail_read_at_call = 0;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_send_helo_fail(void) {
    fail_send_at_call = 0;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_read_helo_fail(void) {
    fail_read_at_call = 1;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_send_mailfrom_fail(void) {
    fail_send_at_call = 1;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_read_mailfrom_fail(void) {
    fail_read_at_call = 2;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_send_rcptto_fail(void) {
    fail_send_at_call = 2;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_read_rcptto_fail(void) {
    fail_read_at_call = 3;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_send_data_fail(void) {
    fail_send_at_call = 3;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_read_data_fail(void) {
    fail_read_at_call = 4;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_send_message_fail(void) {
    fail_send_at_call = 4; /* Body/Message payload step */
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_send_stop_fail(void) {
    fail_send_at_call = 5;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_read_stop_fail(void) {
    fail_read_at_call = 5;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_send_quit_fail(void) {
    fail_send_at_call = 6;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_read_quit_fail(void) {
    fail_read_at_call = 6;
    TEST_ASSERT_EQUAL(1, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

void test_smtp_protocol_success(void) {
    TEST_ASSERT_EQUAL(0, smtp_protcol("f", "t", "s", "b", 25, "h", "srv"));
}

/* --- Runner --- */
int main(void) {
    UNITY_BEGIN();

    /* Basic read_sock permutations */
    RUN_TEST(test_read_sock_success_2xx);
    RUN_TEST(test_read_sock_success_3xx);
    RUN_TEST(test_read_sock_fail_unexpected);
    RUN_TEST(test_read_sock_fail_read);

    /* Connection establishment failures */
    RUN_TEST(test_smtp_socket_fail);
    RUN_TEST(test_smtp_gethostbyname_fail);
    RUN_TEST(test_smtp_connect_fail);
    RUN_TEST(test_smtp_read_greeting_fail);

    /* Protocol progression failures */
    RUN_TEST(test_smtp_send_helo_fail);
    RUN_TEST(test_smtp_read_helo_fail);
    RUN_TEST(test_smtp_send_mailfrom_fail);
    RUN_TEST(test_smtp_read_mailfrom_fail);
    RUN_TEST(test_smtp_send_rcptto_fail);
    RUN_TEST(test_smtp_read_rcptto_fail);
    RUN_TEST(test_smtp_send_data_fail);
    RUN_TEST(test_smtp_read_data_fail);
    RUN_TEST(test_smtp_send_message_fail);
    RUN_TEST(test_smtp_send_stop_fail);
    RUN_TEST(test_smtp_read_stop_fail);
    RUN_TEST(test_smtp_send_quit_fail);
    RUN_TEST(test_smtp_read_quit_fail);

    /* Full protocol loop validation */
    RUN_TEST(test_smtp_protocol_success);

    return UNITY_END();
}