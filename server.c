#include "header.h"


static void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void setnonblocking(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


/*
 * Binding, setting as non-blocking and listening sockfd
 * Returning: sockfd in correct case, -1 in error case
 */
int configure_sockfd(char *str){
    int sockfd;
    struct addrinfo hints; // node that will be filled
    struct addrinfo *servinfo;  // ptr that will point to a result
    struct addrinfo *ptr;       // temp ptr
    int yes = 1; // for setsockopt

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // UNSPEC - no matter IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream-sockets
    hints.ai_flags = AI_PASSIVE;

    // Fill the hints and add info about host in da sefrvinfo
    int status = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (status != 0) { 
        printf("Error. getaddrinfo(): %s\n", gai_strerror(status)); 
        return -1;
    }

    // Binding to the first possible socket
    for (ptr = servinfo; ptr != NULL; ptr = ptr->ai_next){
        sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sockfd == -1){
            perror("Error. Server: socket()");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("Error. Server: setsockopt()");
            continue;
        }
        if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1){
            close(sockfd);
            perror("Error. Server: bind()");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo); 
    setnonblocking(sockfd); 

    // Checking founded connection
    if (ptr == NULL) {
        fprintf(stderr, "Server: failed to bind");
        return -1;
    }

    if (listen(sockfd, LISTENQ) == -1){
        perror("Error. Server: listen()");
        return -1; 
    }

    return sockfd;
}

int main(){

    int sockfd;
    char str[INET6_ADDRSTRLEN]; //Используется в inet_ntop()
    struct sockaddr_storage their_addr;
    socklen_t sin_size = sizeof their_addr;
    
    if ((sockfd = configure_sockfd(str)) == -1){
        printf("\nPlease, restart server.\n");
        exit(1);
    }

    //Epoll tuning
    int epollfd;
    struct epoll_event ev;
    if ((epollfd = epoll_create1(0)) == -1){
        perror("Error. Server: epoll_create1()");
        exit(1);
    }
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP; // EPOLLET - edge-triggered
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1){
        perror("Error. Server: epoll_ctl() - sockfd");
        close(sockfd);
        exit(1);
    }

    // Open database
    sqlite3 *db_users;
    char *err_msg = 0;
    int handle_db_users = sqlite3_open("users.db", &db_users);
    if (handle_db_users != SQLITE_OK){
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_users));
        sqlite3_close(db_users);
        exit(1);
    }

    // Execution of creating table
    char *sql = "CREATE TABLE IF NOT EXISTS users(login TEXT, password TEXT);";
    handle_db_users = sqlite3_exec(db_users, sql, 0, 0, &err_msg); 
    if (handle_db_users != SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_close(db_users);
        exit(1);
    }
    
    // All future clients' states

    // ALTERNATIVE (check it):
    // state_machine clstates = (state_machine*) malloc(MAX_CONNECT * sizeof(state_machine));
    state_machine clstates[MAX_CONNECT];
    for (int i = 0; i < MAX_CONNECT; i++) {
        clstates[i].sockfd = -1;
    }

    struct epoll_event evlist[MAX_EVENTS];
    int nfds; //number of file descriptors

    node *commonroomList = NULL;
    node *alpharoomList = NULL;
    
    
    // Main loop
    while(1){

        nfds = epoll_wait(epollfd, evlist, MAX_EVENTS, -1);
        if (nfds == -1){
            perror("Error. Server: epoll_wait()");
            break;
        }

        for (int i = 0; i < nfds; i++){
            // New client acceptance 
            if (evlist[i].data.fd == sockfd){
                // Accepting connection for the first possible
                int index;
                for (index = 0; index < MAX_CONNECT; index++){
                    if (clstates[index].sockfd == -1){
                        clstates[index].sockfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
                        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), str, sizeof str);                       
                        printf("%d has been connected from %s.\n", clstates[index].sockfd, str);
                        break;
                    }
                }
                if (clstates[index].sockfd == -1){
                    perror("Error. Server: accept()");
                    break;
                }

                setnonblocking(clstates[index].sockfd);
                
                ev.data.fd = clstates[index].sockfd;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clstates[index].sockfd, &ev) == -1){
                    perror("Error. Server: epoll_ctl() add");
                    break;
                }

                clstates[index].state = STATE_START;
                clstates[index].sockfd = clstates[index].sockfd;
                strcpy(clstates[index].login, "\0");

                // Notifications: 
                int count = 0;
                char greet[256] = {'\0'};
				for (int j = 0; j < MAX_CONNECT; j++) {
                    // Counting online members 
					if (clstates[j].sockfd > 0) {
						count++;
					}
				}
                // Sending info 
                // Online now: %d.\nType /signup or /login to continue...
                sprintf(greet,"Connected.\nOnline now: %d.\nType /signup or /login to continue...\n", count);
				if (write(clstates[index].sockfd, greet, strlen(greet)+1) == -1){
                     perror("Error. Server: send()");
                }
            }

            // if client has already connected%d.\n
            else {
                //Checking lost connection
                if (evlist[i].events & EPOLLRDHUP) {
                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, evlist[i].data.fd, 0) == -1){
                        perror("Error. Server: epoll_ctl() - del");
                        exit(1);
                    }

                    // Notifying about client that has left
                    char notify[256] = {'\0'};
                    for (int j = 0; j < MAX_CONNECT; j++){
                        // Closing if it's sock of client that has left 
                        if (clstates[j].sockfd == evlist[i].data.fd){
                            printf("Client %s (sock %d) disconnected.\n", clstates[j].login, clstates[j].sockfd);
                            delete_node(&commonroomList, clstates[j].sockfd);
                            delete_node(&alpharoomList, clstates[j].sockfd);
                            clstates[j].sockfd = -1;
                            sprintf(notify, "%s left.\n", clstates[j].login);
                            memset(clstates[j].login, '\0', MAX_LOGIN_SIZE);
                            if (close(evlist[i].data.fd) == -1){
                                perror("Error. Server: close()");
                            }
                        }
                        // Sending notification if it's other sockets
                        else if (clstates[j].sockfd > 0){
                            send(clstates[j].sockfd, notify, strlen(notify), 0);
                        }
                    }
                }
                //Main part - protocol 
                else if (evlist[i].events & EPOLLIN){   
                    protocol_server(evlist[i].data.fd, clstates, db_users, &commonroomList, &alpharoomList);
                } 
            }
        }
    }

    close(sockfd);
    close(epollfd);
    printf("\nFinishing server...\n");
    exit(0);

}