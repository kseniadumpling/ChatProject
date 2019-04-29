#include "header.h"

/*
 * Determining version (IPv4 or IPv6)
 */
static void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr); //для IPv4
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/*
 * Turning socket into non-blocking mode
 */
static void setnonblocking(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


/*
 * Main func of socket's connection
 * Returning: sockfd in correct case, -1 in error case
 */
int connect_to_server(char *argv[]){
    int sockfd;
    struct addrinfo hints;      // node that will be filled
    struct addrinfo *servinfo;  // ptr that will point to a result
    struct addrinfo *ptr;       // temp ptr
    char str[INET6_ADDRSTRLEN]; // str that will be used it inet_ntop()

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC;     // UNSPEC - no matter IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream-sockets

    // Fill the hints and add info about host in da servinfo
    int status = getaddrinfo(argv[1], PORT, &hints, &servinfo);
    if (status != 0) { 
        printf("getaddrinfo(): %s\n", gai_strerror(status)); 
        return -1;
    }

    // Searching through all results and connecting to the first possible
    for (ptr = servinfo; ptr != NULL; ptr = ptr->ai_next){
        // Getting socket's file descriptor
        sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sockfd == -1){
            perror("Error. Client: socket()");
            continue;
        }
        if (connect(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1){
            close(sockfd);
            perror("Error. Client: connect()");
            continue;
        }
        break;
    }

    // Don't need anymore
    freeaddrinfo(servinfo); 

    // Checking founded connection
    if (ptr == NULL){
        printf("Failed to connect\n");
        return -1;
    }

    // Printing address 
    // ntop = "network to presentation"
    inet_ntop(ptr->ai_family, get_in_addr((struct sockaddr *)ptr->ai_addr), str, sizeof str);
    printf("Client: connecting to %s\n", str);

    return sockfd; 
}


/*
 * Func of tuning epoll
 * Returning: epollfd in correct case, -1 in error case
 */
int configure_epollfd(int sockfd, int pipefd[2]){
    struct epoll_event ev;
    int epollfd;

    epollfd = epoll_create1(0);
    if (epollfd == -1){
        perror("Error. Client: epoll_create1()");
        return -1;
    }

    ev.events = EPOLLIN | EPOLLOUT | EPOLLET; // EPOLLET - edge-triggered
    
    // Linking sockfd & epoll
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1){
        perror("Error. Client: epoll_ctl: sockfd");
        close(sockfd);
        return -1;
    }

    // Linking pipe & epoll
    ev.data.fd = pipefd[0];
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefd[0], &ev) == -1){
        perror("Error. Client: epoll_ctl: pipefd");
        return -1;
    }

    return epollfd;
} 



int main(int argc, char *argv[]) {
    int sockfd; 
    char buf[MAX_DATA_SIZE] = "Hello, World!";

    // Checking arguments (should include address)
    if (argc != 2){
        printf("Error. Usage: client hostname\n");
        exit(1);
    }

    // Connecting
    if ((sockfd = connect_to_server(argv)) == -1){
        exit(1);
    }
    
    // Creating pipe
    int pipefd[2];
    if (pipe(pipefd) == -1){
        perror("Error. Client: pipe");
        exit(1);
    }

    // Epoll tuning
    int epollfd;
    struct epoll_event evpipe[2];

    if ((epollfd = configure_epollfd(sockfd, pipefd)) == -1){
        printf("Please, restart client.\n");
        exit(1);
    }

    //Fork: parent - listen of inputs from server and child, 
    //      child  - waiting for client input
    pid_t pid;
    pid = fork();
    if (pid == -1){
        perror("Error. Client: Fork");
        exit(1);
    }

    int working = 1;
    int res;
    int nfds; // number of file descriptors

    if (pid == 0){
        // child - waiting for client input, sending to pipefd[1]
        close(pipefd[0]);
        while(working){
            memset(buf, 0, MAX_DATA_SIZE);
            fgets(buf, MAX_DATA_SIZE-1, stdin);
            if (write(pipefd[1], buf, MAX_DATA_SIZE-1) == -1){
                perror("Error. Client: send");
            }
        }
    }
    else {
        // parent - listen of inputs from server and child
        close(pipefd[1]);
        while (working){
            nfds = epoll_wait(epollfd, evpipe, 2, -1); 
            if (nfds == -1){
                perror("Error. Client: epoll_wait()");
                break;
            }
            for (int i = 0; i < nfds; i++){
                memset(buf, '\0', MAX_DATA_SIZE);
                // if server send smth:
                if (evpipe[i].data.fd == sockfd){
                    res = recv(sockfd, buf, MAX_DATA_SIZE-1, 0);
                    if (res == -1){
                        perror("Error. Client: recv()");
                        exit(1); 
                    }
                    else if (res == 0){
                        close(sockfd);
                        working = 0;
                        printf("Server closed connection: socket %d.\n",sockfd);
                    }
                    else {
                        printf("%s\n", buf);
                    }

                }
                // if client send smth
                else {
                    res = read(evpipe[i].data.fd, buf, MAX_DATA_SIZE-1);
                    if (res == -1){
                        perror("Error. Client: read()");
                        //exit(1);
                    }
                    else if (res == 0){
                        working = 0;
                    }
                    else {
                        if (send(sockfd, buf, MAX_DATA_SIZE-1, 0) == -1) {
                            perror("Error. Client: send");
                            exit(1);
                        }
                    }
                }
            }
        }
    }

    // Finishing
    if (pid != 0){
        close(pipefd[0]);
        close(sockfd);
    }
    else {
        close(pipefd[1]);
    }

    return 0;
}
