#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


struct ConfigData {
    int port;
};

int config_socket(struct ConfigData config_data);
void child_handler(struct ConfigData *config_data);
