#include "proxy.h"
#include "util.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: ./proxy <port_num>\n");
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
    char hostname[] = "www.facebook.com";
    char header[MAX_HDR_SIZE];
    struct ReqParams req_params;

    // Recv data from the client
    recv_header(clientfd, header);

    // Parse request
    parse_request(&req_params, header);

    // TODO: Check if the requested data is cached

    // TODO: Retrieve data from server
    retrieve_data(clientfd, hostname, header, &req_params);

    // TODO: Send data back to client

    // TODO: Cleanup and free memory yo

    return;
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


int retrieve_data(int sock, char *hostname, char *recv_buff, struct ReqParams *req_params)
// int retrieve_data(char *hostname, char *recv_buff, struct ReqParams *req_params)
{
    char dest_ip[INET_ADDRSTRLEN];
    char get_data[MAX_BUF_SIZE];
    int csock, rebytes;

    // Perform DNS lookup for the requested url
    if (dnslookup(req_params->uri, dest_ip, sizeof(dest_ip)) < 0) {

        // A 400 error needs to be thrown here
        printf("Bad URL: %s\n", req_params->uri);
        return -1;
    }

    // Create socket with connection to url's IP
    if ((csock = create_socket(dest_ip)) <=  0) {
        printf("Something bad happened\n");
        exit(-1);
    }

    // Send original request from client to the server
    int sebytes = send(csock, recv_buff, MAX_HDR_SIZE, 0);
    printf("SENT %d BYTES\n", sebytes);

    // Receive the data
    while ((rebytes = recv(csock, get_data, sizeof(get_data), 0)) > 0) {
        printf("RECEIVED: %d bytes\n", rebytes);
        send(sock, get_data, rebytes, 0);
    }

    // Cleanup
    close(csock);


    return 1;
}


// Parse GET request from client and store into struct
void parse_request(struct ReqParams *req_params, char *recv_buff)
{
    char *token;
    char *line;

    // Split lines
    token = strtok(strdup(recv_buff),"\n");

    // Only relevant information is in the first line, remove http:// prefix
    line = strdup(token);
    remove_elt(line, "http://");

    // Parse the three fields delimited by space in the line
    req_params->method = strdup(strtok(line, " "));
    req_params->uri = strdup(strtok(NULL, "/"));
    strtok(NULL, " ");
    req_params->version = strdup(strtok(NULL, " "));

    // Ugh stupid trailing random bytes
    req_params->version[8] = '\0';

    // printf("Method: %s\n", req_params->method);
    // printf("URI: %s\n", req_params->uri);
    // printf("VERSION: %s\n", req_params->version);

    free(token);
    free(line);
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
            config_data->timeout = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-e") == 0) {
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


void print_config(struct ConfigData *config_data)
{
    // Print all configurations options prettttty
    printf("*************** t2PROXY ****************\n");
    printf("*       PORT: %d\n", config_data->port);
    printf("*    TIMEOUT: %d s\n", config_data->timeout);
    printf("* EXPIRATION: %d ms\n", config_data->exp_timeout);
    printf("****************************************\n\n");
}
void remove_elt(char *og_str, const char *sub_str)
{
    while((og_str=strstr(og_str,sub_str)))
    {
        memmove(og_str,og_str+strlen(sub_str),1+strlen(og_str+strlen(sub_str)));
    }
}
