#include "lab.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#ifdef TEST
#define main main_exclude
#endif



int main(void)
{
    open_close(2525, "ec2-54-148-3-55.us-west-2.compute.amazonaws.com", "Hello World");
    return 0;
}