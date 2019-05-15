#include "header.h"

//Определение версии (IPv4 or IPv6)
void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr); //для IPv4
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void setnonblocking(int fd) {
	int flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

int main(int argc, char *argv[]) {
    int sockfd; //Файловый дескриптор
    int numbytes; //Длина сообщения
    char buf[MAX_DATA_SIZE] = "Hello, World!";
    struct addrinfo hints; //Узел, который будем заполнять
    struct addrinfo *servinfo; //Указывает на результат
    struct addrinfo *p;
    char s[INET6_ADDRSTRLEN]; //Используется в inet_ntop()


    //Проверим аргументы
    if (argc != 2){
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints); //Обнулим все значения нового узла
    hints.ai_family = AF_UNSPEC; //UNSPEC - любой, IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; //Потоковый

    //getaddrinfo создает узел, возвращает инф об имени отдельного хоста,
    //заменяет старые функции gethostbyname() getservbyname()
    int rv = getaddrinfo(argv[1], PORT, &hints, &servinfo);
    if (rv != 0) { //Создаем и проверяем корректность создания узла структуры addrinfo
        fprintf(stderr, "Error. getaddrinfo(): %s\n", gai_strerror(rv)); //gai_strerror - печатная версия кода возврата при ошибке
        return 1;
    }

    //Цикл по всем результатам и связывание с первым возможным
    for (p = servinfo; p != NULL; p = p->ai_next){
        //Получаем (и проверяем) дескриптор
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1){
            perror("Error. Client: socket()");
            continue;
        }
        //printf("\nSocket num: %d\n", sockfd);
        //Подключаемся
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("Error. Client: connect()");
            continue;
        }
        break;
    }

    //Проверяем, нашли ли соединение
    if (p == NULL){
        fprintf(stderr, "Error. Client: failed to connect\n");
        return 2;
    }

    //Creating pipe
    int pipefd[2];
    if (pipe(pipefd) == -1){
        perror("Error. Client: pipe");
        exit(1);
    }


    //Выведем адрес подключения
    // ntop = "network to presentation"
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("Client: connecting to %s\n", s);

    freeaddrinfo(servinfo); //Don't need anymore

    //Epoll tuning
    struct epoll_event ev, evpipe[2];
    struct epoll_event evlist[MAX_EVENTS];
    int epollfd;
    int nfds, i;
  
    epollfd = epoll_create1(0);
    if (epollfd == -1){
        perror("Error. Client: epoll_create1()");
        exit(1);
    }
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1){
        perror("Error. Client: epoll_ctl: sockfd");
        close(sockfd);
        exit(1);
    }

    //Plugging in pipe & epoll
    ev.data.fd = pipefd[0];
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefd[0], &ev) == -1){
        perror("Error. Client: epoll_ctl: sockfd");
        close(sockfd);
        exit(1);
    }

    //Fork: parent - listen of inputs from server and child, 
    //child - waiting for client input
    pid_t pid;
    pid = fork();
    if (pid == -1){
        perror("Error. Client: Fork");
        exit(1);
    }

    int working = 1;
    int res;
    if (pid == 0){
        // child - ожидание ввода сообщения от пользователя, передача в pipefd[1]
        close(pipefd[0]);
        while(working){
            memset(buf, 0, MAX_DATA_SIZE);
            fgets(buf, MAX_DATA_SIZE-1, stdin);
            if (write(pipefd[1], buf, MAX_DATA_SIZE-1) == -1){
                perror("Error. Client: send");
                //exit(1);
            }
        }
    }
    else {
        // parent - ожидает ввода и от сервера, и от дочернего процесса
        close(pipefd[1]);
        while (working){
            nfds = epoll_wait(epollfd, evpipe, 2, -1); 
            if (nfds == -1){
                perror("Error. Client: epoll_wait()");
                exit(1);
            }
            for (i = 0; i < nfds; i++){
                memset(buf, '\0', MAX_DATA_SIZE);
                
                /*
                 * Подумать насчет того, что мы не можем 
                 * принимать сообщения до того момента, 
                 * пока не появимся в каком-либо руме
                 * (конкретно с приватными чатрумами разруливать,
                 * я подозреваю, нужно будет на стороне сервера)
                 */


                // if server send smth:
                if (evpipe[i].data.fd == sockfd){
                    res = recv(sockfd, buf, MAX_DATA_SIZE-1, 0);
                    
                    /*
                     * Вставить структуру, следить за формой ввода, 
                     * не вводить сообщения в состоянии LOGIN, PASSWORD,
                     * SINGUP 
                     */

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
    if (pid != 0){
        close(pipefd[0]);
        close(sockfd);
    }
    else {
        close(pipefd[1]);
    }
    return 0;
}
