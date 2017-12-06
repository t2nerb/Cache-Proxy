#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#define MAX_NS_SIZE 128
#define MAX_BUF_SIZE 512
#define MAX_HDR_SIZE 8192
#define HOST_NAME_MAX 64


struct ConfigData {
    int port;
    int timeout;
    unsigned int exp_timeout;
    char *nsblock[MAX_NS_SIZE];
};

struct ReqParams {
    char *host;
    char *uri;
    char *port;
    char *version;
    char *method;
    char *ctype;
};

int config_socket(int port);
void remove_elt(char *og_str, const char *sub_str);
int create_socket(char *dest_ip);
void child_handler(int clientfd, struct ConfigData *config_data);
void parse_cla(struct ConfigData *config_data, int argc, char *argv[]);
void print_config(struct ConfigData *config_data);
void parse_request(struct ReqParams *req_params, char *recv_buff);
// int retrieve_data(char *hostname, char *recv_buff, struct ReqParams *req_params);
int retrieve_data(int sock, char *hostname, char *recv_buff, struct ReqParams *req_params);
void recv_header(int clientfd, char *recv_buff);

