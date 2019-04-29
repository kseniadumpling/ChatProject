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

    // Fill the hints and add info about host in da servinfo
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

    char buf[MAX_DATA_SIZE] = "Hello, World!";
    char msg[MAX_DATA_SIZE] = "Msg for World!";


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

    /*  Here goes segmentation fault. 
        Check sqlite.org/cintro.html, 
        mb answer will be there

    sqlite3 *db_users;
    char *sql = "CREATE TABLE IF NOT EXISTS users(login TEXT, password TEXT);" ;
    if (execute_db(db_users, "users.db", sql) == -1){
        printf("Error with database users.db\n");
        exit(1);
    } 
    */ 

    /*
     * TODO: Rewrite this part (working with DB). 
     *       There is a great field for bugs coz database 
     *       will never be closed. This code only for 
     *       testing some func in the protocol
     */

    // Open database
    sqlite3 *db_users;
    char *err_msg = 0;
    int handle_db_users = sqlite3_open("users.db", &db_users);
    if (handle_db_users != SQLITE_OK){
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db_users));
        sqlite3_close(db_users);
    }
    // Execution of creating table
    char *sql = "CREATE TABLE IF NOT EXISTS users(login TEXT, password TEXT);";
    handle_db_users = sqlite3_exec(db_users, sql, 0, 0, &err_msg); 
    if (handle_db_users != SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_close(db_users);
    }
    
    
    // All future clients' sockets
    int clsock[MAX_CONNECT];
    for (int i = 0; i < MAX_CONNECT; i++) {
        clsock[i] = -1;
    }

    // All future clients' states
    state_machine clstates[MAX_CONNECT];

    struct epoll_event evlist[MAX_EVENTS];
    int nfds; //number of file descriptors

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
                    if (clsock[index] == -1){
                        clsock[index] = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
                        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), str, sizeof str);                       
                        printf("%d has been connected from %s.\n", clsock[index], str);
                        break;
                    }
                }
                if (clsock[index] == -1){
                    perror("Error. Server: accept()");
                    break;
                }

                setnonblocking(clsock[index]);
                
                ev.data.fd = clsock[index];
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clsock[index], &ev) == -1){
                    perror("Error. Server: epoll_ctl() - clsock");
                    break;
                }

                clstates[index].state = STATE_START;
                clstates[index].sockfd = clsock[index];
                clstates[index].login = "undefined";

                // Notifications: 
                char greet[256];
                sprintf(greet,"Connected.\nYour id is %d.\nMembers' id online: ", clsock[index]);
                char notify[256];
				sprintf(notify, "%d joined.\n", clsock[index]);
				for (int j = 0; j < MAX_CONNECT; j++) {
                    // TODO: rewrite, using info from DB

                    // Adding all of IDs to string 
					if (clsock[j] > 0) {
						sprintf(greet, "%s %d",greet, clsock[j]);
					}
                    // Sending notification to other clients
                    if (clsock[j] > 0 && j != index){
                        send(clsock[j], notify, strlen(notify), 0);
                    }
				}
                // Sending info 
				sprintf(greet, "%s\n\nType /signup or /login to continue...\t", greet); // kinda к о с т ы л ь
				if (send(clsock[index], greet, strlen(greet), 0) == -1){
                     perror("Error. Server: send()");
                }
            }

            // if client has already connected
            else {
                //Checking lost connection
                if (evlist[i].events & EPOLLRDHUP) {
                    printf("Clinent %d disconnected.\n", evlist[i].data.fd);
                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, evlist[i].data.fd, 0) == -1){
                        perror("Error. Server: epoll_ctl() - del");
                        exit(1);
                    }

                    // Notifying about client that has left
                    char notify[256];
					sprintf(notify, "%d left.\n",evlist[i].data.fd);
                    for (int j = 0; j < MAX_CONNECT; j++){
                        // Closing if it's sock of client that has left 
                        if (clsock[j]== evlist[i].data.fd){
                            clsock[j] = -1;
                            if (close(evlist[i].data.fd) == -1){
                                perror("Error. Server: close()");
                            }
                        }
                        // Sending notification if it's other sockets
                        else if (clsock[j] > 0){
                            send(clsock[j], notify, strlen(notify), 0);
                        }
                    }
                }
                //Main part - protocol
                else if (evlist[i].events & EPOLLIN) {
                    protocol_server(evlist[i].data.fd, clsock, clstates, db_users, buf, msg);
                } 
            }
        }
    }
    close(sockfd);
    close(epollfd);
    printf("\nFinishing server...\n");
    exit(0);

}