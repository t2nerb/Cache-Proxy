#include "proxy.h"

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

    // Configure socket
    config_data.port = atoi(argv[1]);
    sockfd = config_socket(config_data);

    // Wait for incoming connections
	for (;;) {
		int clientfd;
		int pid;

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
            child_handler(&config_data);

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
// Cache some shizz
void child_handler(struct ConfigData *config_data)
{
    printf("I'm in child now yaya!\n");
    sleep(3);
}


// Bind to socket and listen on port
int config_socket(struct ConfigData config_data)
{
    int sockfd;
    int enable = 1;
    struct sockaddr_in remote;

    // Populate sockaddr_in struct with configuration for socket
    bzero(&remote, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(config_data.port);
    remote.sin_addr.s_addr = INADDR_ANY;

    // Create a socket of type TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Could not create socket: ");
        exit(-1);
    }

    // Set socket option for fast-rebind socket
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
        perror("Unable to set sock option: ");

    if ((bind(sockfd, (struct sockaddr *) &remote, sizeof(struct sockaddr_in))) < 0) {
        perror("Could not bind to socket: ");
        exit(-1);
    }

    // Allow up to 1 simultaneous connections on the socket
    if ((listen(sockfd, 1)) < 0) {
        perror("Could not listen: ");
        exit(-1);
    }

    return sockfd;
}
