#include "proxy.h"
#include "util.h"


int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: ./proxy <port_num> -t <timeout>\n");
        exit(-1);
    }

    int sockfd;
    unsigned int client_len;
    struct ConfigData config_data;
    struct sockaddr_in client;
    client_len = sizeof(struct sockaddr_in);

    // Parse command line arguments
    parse_cla(&config_data, argc, argv);

    // Print configuration options
    print_config(&config_data);

    // Create cache dir if it doesn't exist
    create_cachedir();

    // Configure socket
    sockfd = config_socket(config_data.port);

    // Wait for incoming connections
    for (;;) {
        int clientfd, pid;

        // Accept new connections and store file descriptor
        clientfd = accept(sockfd, (struct sockaddr *) &client, &client_len);
        if (clientfd < 0) {
            perror("Could not accept: ");
            exit(-1);
        }

        pid = fork();
        if (pid < 0) {
            perror("Could not fork: ");
            exit(-1);
        }

        // I am the child
        if (pid == 0) {
            close(sockfd);

            // Call child handler here
            child_handler(clientfd, &config_data);

            exit(0);
        }

        // I am the parent
        if (pid > 0) {
            close(clientfd);
            waitpid(0, NULL, WNOHANG);
        }
    }

    return 0;
}


// The initial request will be from client
// Data the client requests may need to be cached
// Forward the client's request to the proper server i.e (google.com)
// Cache the data received from server and set timeout value
void child_handler(int clientfd, struct ConfigData *config_data)
{
    char header[MAX_HDR_SIZE];
    char *header_hash;
    struct ReqParams req_params;

    // Recv data from the client
    recv_header(clientfd, header);

    // Parse request
    if (parse_request(&req_params, header) != 0) {
        close(clientfd);
        exit(0);
    }

    // Check if IP is blocked
    check_if_blocked(req_params.uri);

    // Hash the request header
    header_hash = md5_string(header);

    // Check if the requested data is cached
    if (search_cache(header_hash, config_data) == 0) {
        // If not retrieve data from server, write to cache and send
        retrieve_data(clientfd, header, req_params.uri, header_hash);
    }
    else {
        // Send data from cache to client
        send_file(clientfd, header_hash);
    }


    // Cleanup and free memory yo
    close(clientfd);

    return;
}


void check_if_blocked(char *address)
{
    FILE *fp;
    char conf_line[MAX_BUF_SIZE];

    fp = fopen("./blocklist.txt", "r");
    if (!fp) {
        perror("Could not find blocklist");
        return;
    }

    while (fgets(conf_line, sizeof(conf_line), fp)) {

        // Remove crap from hostname; Should not affect IP addresses
        remove_elt(conf_line, "http");
        remove_elt(conf_line, "/");
        remove_elt(conf_line, ":");

        if (strncmp(address, conf_line, strlen(address)) == 0) {
            printf("BLOCKED: %s\n", address);
            exit(0);
        }

    }

}


void send_file(int clientfd, char *header_hash)
{
    char fpath[MAX_FP_SIZE];
    char buffer[MAX_BUF_SIZE];
    FILE *fp;
    int rbytes;

    strcpy(fpath, CACHE_DIRECTORY);
    strcat(fpath, header_hash);

    fp = fopen(fpath, "rb");
    while ((rbytes = fread(buffer, 1, MAX_BUF_SIZE, fp)) > 0) {
        send(clientfd, buffer, rbytes, 0);
    }

    return;
}

int search_cache(char *header_hash, struct ConfigData *config_data)
{
    // struct stat attrib;
    char fpath[MAX_FP_SIZE];
    struct stat fileinfo;
    time_t mytime;
    double diff;

    strcpy(fpath, CACHE_DIRECTORY);
    strcat(fpath, header_hash);

    // Check if file exists
    if( access(fpath, F_OK ) != -1 ) {

        // Get age of file
        stat(fpath, &fileinfo);
        time(&mytime);
        ctime(&fileinfo.st_mtime);

        diff = difftime(mytime, fileinfo.st_mtime);
        if (diff > config_data->exp_timeout) {
            printf("EXPIRED: %s\n", header_hash);
            return 0;
        }
        else {
            return 1;
        }

    }
    else {
        return 0;
    }
}


void recv_header(int clientfd, char *recv_buff)
{
    int rebytes;
    int offset = 0;

    if ((rebytes = recv(clientfd, recv_buff + offset, MAX_HDR_SIZE, 0)) > 0) {
        // printf("READ %d BYTES FOR HEADER\n");
    }

    return;
}

int lookup_ip(char *url, char *dest_ip)
{
    char cache_line[MAX_BUF_SIZE];
    char write_line[MAX_BUF_SIZE];
    char *cache_format = "%s*%s\n";
    FILE *hostname_fp, *write_fp;

    write_fp = fopen(HOSTS_PATH, "a");
    hostname_fp = fopen(HOSTS_PATH, "r");

    // Check if url has IP cached in file
    while (fgets(cache_line, sizeof(cache_line), hostname_fp)) {
        char *hostname, *ip_addr;

        hostname = strtok(cache_line, "*");
        ip_addr = strtok(NULL, "\n");

        if (strcmp(hostname, url) == 0) {
            strcpy(dest_ip, ip_addr);
            return 1;
        }

    }

    // Perform dnslookup on url
    if (dnslookup(url, dest_ip, INET_ADDRSTRLEN) < 0) {
        printf("BAD URL: %s\n", url);
        return -1;
    }

    // Write the hostname:ip_addr pair to file
    snprintf(write_line, sizeof(write_line), cache_format, url, dest_ip);
    fwrite(write_line, 1, strlen(write_line), write_fp);

    // Cleanup
    fclose(hostname_fp);
    fclose(write_fp);

    return 1;
}


int retrieve_data(int sock, char *recv_buff, char *url, char *hash)
{
    char dest_ip[INET_ADDRSTRLEN];
    char buff[MAX_BUF_SIZE];
    char fpath[MAX_FP_SIZE];
    int csock, rebytes, total_bytes = 0;

    if (lookup_ip(url, dest_ip) < 0) {
        return -1;
    }

    // Perform DNS lookup for the requested url
    // if (dnslookup(url, dest_ip, sizeof(dest_ip)) < 0) {
    //     // A 400 error needs to be thrown here
    //     printf("Bad URL: %s\n", url);
    //     return -1;
    // }

    // Check if IP is blocked
    check_if_blocked(dest_ip);

    // Create socket with connection to url's IP
    if ((csock = create_socket(dest_ip)) <=  0) {
        printf("Something bad happened\n");
        exit(-1);
    }

    // Send original request from client to the server
    safe_send(csock, recv_buff, MAX_HDR_SIZE);

    // Create path for cache
    strcpy(fpath, CACHE_DIRECTORY);
    strcat(fpath, hash);
    printf("CACHED CONTENT: %s\n", hash);

    // Receive the data and write to file
    FILE *cachefp = fopen(fpath, "wb");
    while ((rebytes = recv(csock, buff, sizeof(buff), 0)) > 0) {
        safe_send(sock, buff, rebytes);
        fwrite(buff, 1, rebytes, cachefp);
        // printf("%s", strtok(buff, "\r\n"));
        total_bytes += rebytes;
    }

    // Cleanup
    close(csock);
    fclose(cachefp);


    return 1;
}


// Ensure that nbytes of content is sent to outsock
void safe_send(int outsock, char *content, int nbytes)
{
    int sebytes;

    sebytes = send(outsock, content, nbytes, 0);
    while (sebytes < nbytes) {
        sebytes += send(outsock, content+sebytes, nbytes-sebytes, 0);
    }
    return;
}


// Parse GET request from client and store into struct
int parse_request(struct ReqParams *req_params, char *recv_buff)
{
    char *token;
    char *hostline;

    // Split lines
    token = strtok_r(strdup(recv_buff),"\r\n", &hostline);

    // Get method from firstline
    req_params->method = strdup(strtok(token, " "));

    // Get hostname from next line 
    strtok(hostline, " ");
    req_params->uri = strdup(strtok(NULL, "\r\n"));

    // Cleanup
    free(token);

    return (strcmp(req_params->method, "GET") == 0) ? 0 : -1;
}


void create_cachedir()
{
    FILE *hostname_fp;

    // If directory doesn't exist, make the directory
    mkdir(CACHE_DIRECTORY, 0700);
    printf("CREATED CACHE DIRECTORY: %s\n", CACHE_DIRECTORY);

    // Create hostname file
    hostname_fp = fopen(HOSTS_PATH, "w");
    fprintf(hostname_fp, "localhost*127.0.0.1\n");
    fclose(hostname_fp);
    printf("CREATED HOSTNAME CACHE: %s\n", HOSTS_PATH);

}


// Parse the configurable parameters from command line arguments
// EXAMPLE FORMAT: ./proxy 8000 -t 60 -e 100
// -t (timeout value) is in seconds
// -e (expiration timeout) is in microseconds
void parse_cla(struct ConfigData *config_data, int argc, char *argv[])
{
    // Port will always be the first arg
    config_data->port = atoi(argv[1]);

    // Iterate through args
    for (int i = 2; i < argc; i++) {

        if (strcmp(argv[i], "-t") == 0) {
            config_data->exp_timeout = atoi(argv[++i]);
        }
    }

    return;
}


// Bind to socket and listen on port
int config_socket(int port)
{
    int sockfd;
    int enable = 1;
    struct sockaddr_in remote;

    // Populate sockaddr_in struct with configuration for socket
    bzero(&remote, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr.s_addr = INADDR_ANY;

    // Create a socket of type TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Could not create socket: ");
        exit(-1);
    }

    // Set socket option for fast-rebind socket
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("Unable to set sock option: ");
    }

    if ((bind(sockfd, (struct sockaddr *) &remote, sizeof(struct sockaddr_in))) < 0) {
        perror("Could not bind to socket: ");
        exit(-1);
    }

    // Allow up to 4 simultaneous connections on the socket
    if ((listen(sockfd, 4)) < 0) {
        perror("Could not listen: ");
        exit(-1);
    }

    return sockfd;
}

int create_socket(char *dest_ip)
{
    struct sockaddr_in server;
    struct timeval tv;

    // Populate server struct
    server.sin_addr.s_addr = inet_addr(dest_ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(80);

    // Populate sockopts
    tv.tv_sec = 3;  // recv timeout in seconds
    tv.tv_usec = 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Socket could not be created\n");
        return -1;
    }

    // set sockoption to timeout after blocking for 3 seconds on recv
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv,sizeof(struct timeval));

    // Try to connect
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Could not connect\n");
        return -1;
    }

    return sock;
}

char *md5_string(char* buffer)
{
    unsigned char digest[16];
    char *tmp, *firstline;
    char *hash;

    // Relevant data retrieval is determined in first line
    tmp = strdup(buffer);
    firstline = strtok(tmp, "\n");

    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, firstline, strlen(firstline));
    MD5_Final(digest, &ctx);

    char mdString[33];
    for (int i = 0; i < 16; i++)
        sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);
    hash = malloc(strlen(mdString));
    strcpy(hash, mdString);

    free(tmp);

    return hash;
}

void print_config(struct ConfigData *config_data)
{
    // Print all configurations options prettttty
    printf("*************** t2PROXY ****************\n");
    printf("*       PORT: %d\n", config_data->port);
    printf("* EXPIRATION: %d s\n", config_data->exp_timeout);
    printf("*   CACHEDIR: ./Cache\n");
    printf("****************************************\n\n");
}
void remove_elt(char *og_str, const char *sub_str)
{
    while((og_str=strstr(og_str,sub_str)))
    {
        memmove(og_str,og_str+strlen(sub_str),1+strlen(og_str+strlen(sub_str)));
    }
}
