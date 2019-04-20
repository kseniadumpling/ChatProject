#include "header.h"


void *get_in_addr(struct sockaddr *sa){
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void setnonblocking(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}


int main(){
    int sockfd;
    struct addrinfo hints; //Узел, который будем заполнять
    struct addrinfo *servinfo, *p;
    int rv;
    int yes=1;
    char s[INET6_ADDRSTRLEN]; //Используется в inet_ntop()
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char cnct[MAX_DATA_SIZE] = "Connected.";
    char buf[MAX_DATA_SIZE] = "Hello, World!";
    char msg[MAX_DATA_SIZE] = "Msg for World!";


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;


    // Binding & listening socket
    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo))!= 0){ 
        fprintf(stderr, "Error. getaddrinfo(): %s\n", gai_strerror(rv));
        return -1;
    }
    for (p = servinfo; p != NULL; p = p->ai_next){
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1){
            perror("Error. Server: socket()");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1){
            perror("Error. Server: setsockopt()");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("Error. Server: bind()");
            continue;
        }
        break;
    }
    setnonblocking(sockfd); 
    if (p == NULL) {
        fprintf(stderr, "Server: failed to bind");
        return 2;
    }
    freeaddrinfo(servinfo); // Don't need anymore
    if (listen(sockfd, LISTENQ) == -1){
        perror("Error. Server: listen()");
        exit(1);
    }

    //Epoll tuning
    int epollfd;
    struct epoll_event ev;
    struct epoll_event evlist[MAX_EVENTS];
    int nfds, i;
    if ((epollfd = epoll_create1(0)) == -1){
        perror("Error. Server: epoll_create1()");
        exit(1);
    }
    //in & edge-triggered
    ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1){
        perror("Error. Server: epoll_ctl() - sockfd");
        close(sockfd);
        exit(1);
    }
    
    //Open database
    sqlite3 *dbUsers;
    char *err_msg = 0;
    int handleUsers = sqlite3_open("users.db", &dbUsers);
    if (handleUsers != SQLITE_OK){
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(dbUsers));
        sqlite3_close(dbUsers);
    }
    //Execution of creating table
    char *sql = "CREATE TABLE IF NOT EXISTS users(login TEXT, password TEXT);" ; // INSERT INTO users VALUES('admin', 'admin');
    handleUsers = sqlite3_exec(dbUsers, sql, 0, 0, &err_msg); 

    //clsock = all clients' sockets
    int clsock[MAX_CONNECT];
    for (int i = 0; i < MAX_CONNECT; i++) {
        clsock[i] = -1;
    }

    state_machine clstates[MAX_CONNECT];
    int numOfClient = 0;

    //Main loop
    while(1){
        //nfds = number of file descriptors
        nfds = epoll_wait(epollfd, evlist, MAX_EVENTS, -1);
        if (nfds == -1){
            perror("Error. Server: epoll_wait()");
            exit(1);
        }
        for (i=0; i < nfds; i++){
            //New client acceptance 
            if(evlist[i].data.fd == sockfd){
                sin_size = sizeof their_addr;
                int index = 0;
                for (index = 0; index < MAX_CONNECT; index++){
                    if (clsock[index] == -1){
                        clsock[index] = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
                        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);                       
                        printf("%d has been connected from %s.\n", clsock[index], s);
                        break;
                    }
                }
                if (clsock[index] == -1){
                    perror("Error. Server: accept()");
                    close(sockfd);
                    exit(1);
                }
                setnonblocking(clsock[index]);
                ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLOUT;
                ev.data.fd = clsock[index];
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clsock[index], &ev) == -1){
                    perror("Error. Server: epoll_ctl() - clsock");
                    close(sockfd);
                    close(epollfd);
                    exit(1);
                }
                clstates[index].state = STATE_START;
                clstates[index].sockfd = clsock[index];

                //Notification about connection &
                //greeting (for new client window)
                char greet[256];
                sprintf(greet,"%s\nYour id is %d.\nMembers' id online: ", cnct, clsock[index]);

                //Notification (all other clients)
                char notify[256];
				sprintf(notify, "%d joined.\n", clsock[index]);
				for (int j = 0; j < MAX_CONNECT; j++) {
                    //Adding all of IDs to string 
                    // !!! Переписать с подключением БД и выводом кол-ва человек онлайн
					if (clsock[j] > 0) {
						sprintf(greet, "%s %d",greet, clsock[j]);
					}
                    //Sending notification to other clients
                    if (clsock[j] > 0 && j != index){
                        send(clsock[j], notify, strlen(notify), 0);
                    }
				}
                //Sending info 
				sprintf(greet, "%s\n\nType /signup or /login to continue...\t", greet); // kinda к о с т ы л ь
				send(clsock[index], greet, strlen(greet), 0);
            }
            else{
                //Checking lost connection
                if (evlist[i].events & EPOLLRDHUP) {
                    printf("Clinent %d disconnected.\n", evlist[i].data.fd);
                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, evlist[i].data.fd, 0) == -1){
                        perror("Error. Server: epoll_ctl() - del");
                        exit(1);
                    }
                    //Notification about client that left
                    char notify[256];
					sprintf(notify, "%d left.\n",evlist[i].data.fd);
                    for (int j = 0; j < MAX_CONNECT; j++){
                        //Closing socket
                        if (clsock[j]== evlist[i].data.fd){
                            clsock[j] = -1;
                            if (close(evlist[i].data.fd) == -1){
                                perror("Error. Server: close()");
                            }
                        }
                        //Sending notification
                        else if (clsock[j] > 0){
                            send(clsock[j], notify, strlen(notify), 0);
                        }
                    }
                }
                //Main part - protocol
                else if (evlist[i].events & EPOLLIN) {
                    protocol_server(evlist[i].data.fd, clsock, clstates, buf, msg);
                } 
            }
        }
    }
    close(sockfd);
    close(epollfd);
    printf("\nFinishing server...\n");
    exit(0);

}