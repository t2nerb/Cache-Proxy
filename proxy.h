#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <openssl/md5.h>

#define MAX_NS_SIZE 128
#define MAX_BUF_SIZE 512
#define MAX_FP_SIZE 64
#define MAX_HDR_SIZE 8192
#define HOST_NAME_MAX 64


struct ConfigData {
    int port;
    unsigned int exp_timeout;
};

struct ReqParams {
    char *host;
    char *uri;
    char *method;
};

struct CacheParams {
    char *hash;
    unsigned int time;
};

int config_socket(int port);
int search_cache(char *header_hash);
int create_socket(char *dest_ip);
int parse_request(struct ReqParams *req_params, char *recv_buff);
int retrieve_data(int sock, char *recv_buff, char *url, char *hash);
int lookup_ip(char *url, char *dest_ip);
char *md5_string(char* buffer);
void recv_header(int clientfd, char *recv_buff);
void safe_send(int outsock, char *content, int nbytes);
void remove_elt(char *og_str, const char *sub_str);
void child_handler(int clientfd, struct ConfigData *config_data);
void parse_cla(struct ConfigData *config_data, int argc, char *argv[]);
void print_config(struct ConfigData *config_data);
void send_file(int clientfd, char *header_hash);
void check_if_blocked(char *uri);
void create_cachedir();
