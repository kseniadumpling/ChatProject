#include "header.h"


state_status get_cmd_type(char *cmdargs) {
    if (strncmp(cmdargs, "/login", 6) == 0)
        return STATE_LOGIN;

    if (strncmp(cmdargs, "/signup", 7) == 0)
        return STATE_SIGNUP;

    if (strncmp(cmdargs, "/help", 5) == 0)
        return STATE_HELP;

    if (strncmp(cmdargs, "/list", 5) == 0)
        return STATE_LIST;

    if (strncmp(cmdargs, "/connect", 8) == 0)
        return STATE_CONNECT;

    if (strncmp(cmdargs, "/logout", 7) == 0)
        return STATE_LOGOUT;

    if (strncmp(cmdargs, "/exit", 5) == 0)
        return STATE_EXIT;

    return STATE_UNKNOWN;
}

void protocol_server(int active_socket, int *clsockets, state_machine *clstates,
                        char *buf, char *msg) {
    memset(buf, '\0', MAX_DATA_SIZE);
    memset(msg, '\0', MAX_DATA_SIZE);
    int index;
    // Finding right index
    for (index = 0; index < MAX_CONNECT; index++) {
        if (clstates[index].sockfd == active_socket){
            break;
        }
    }
    switch (clstates[index].state){
        case STATE_START:
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
            }
            printf("%d: %s", active_socket, buf);
            if (strncmp(buf, "/", 1) == 0){
                clstates[index].state = get_cmd_type(buf);
                if (clstates[index].state == -1){
                    printf("%d: Unknown command\n", active_socket);
                    sprintf(msg, "Unknown command. Please, type /signup or /login\t");
                    if(send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_START;
                }
                else if (clstates[index].state  == STATE_LOGIN){ 
                    sprintf(msg, "\nPlease, write login:\t");
                    if(send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_LOGIN;
                }
                else if (clstates[index].state  == STATE_SIGNUP){
                    sprintf(msg, "\nPlease, create new login:\t");
                    if(send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_SIGNUP;
                }
            }
            else {
                printf("%d: undefined user.\n", active_socket);
                sprintf(msg, "Please, sign up or log in\t");
                if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                    perror("Error. Server: send()");
                }
            }
            break; 
            /* End of case START */
        
        case STATE_SIGNUP:
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }

            /*
             * Проверка на корректность введенного логина
             * (ввести правила, что логин начинается с букв,
             *  красиво было бы прикрутить регулярные выражение)
             */

            printf("New login %d: %s", active_socket, buf);
            /* INSERT;
                DB                            
            */
            sprintf(msg, "Please, create password:\t");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Srever: send()");
                break;
            }
            clstates[index].state = STATE_SIGNUP_PASS;
            break;
            /* End of case STATE_SIGNUP */

        case STATE_SIGNUP_PASS:
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
             /* INSERT;
                DB                            
            */
            printf("New password %d: %s", active_socket, buf);
            printf("New profile created successfully\n");
            sprintf(msg, "Success! Welcome to the common room\n");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Srever: send()");
                break;
            }
            clstates[index].state = STATE_COMMONROOM;
            break;
            /* End of case STATE_SIGNUP_PASS */


        case STATE_LOGIN: 
            /* kinda к о с т ы л ь
            sprintf(msg, "Please, write login:\n");
            if(send(active_socket, msg, MAX_DATA_SIZE-1, 0)==-1){
                perror("Error. Srever: send()");
                break;
            }*/
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
            printf("Login %d: %s", active_socket, buf); //msg to server
            /* SELECT * FROM users.db найти номер строки логина, add to 
                if incorrect - send error msg, break;                            
            */

            sprintf(msg, "Please, write password:\t");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Srever: send()");
                break;
            }
            clstates[index].state = STATE_PASSWORD;
            break;
            /* End of case STATE_LOGIN */

        case STATE_PASSWORD:
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                exit(1);
            }
            printf("Password %d: %s", active_socket, buf); //msg to server
            /* SELECT * FROM users.db сравнить пароль у логина в строке 
                if incorrect - esnd error msg, break;
            */
            clstates[index].state = STATE_COMMONROOM;

            sprintf(msg, "\nCorrect.\nType /help to see all available commands\n"); 
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
            }
            break;
            /* End of case STATE_PASSWORD */

        case STATE_COMMONROOM: 
            //memset(buf, '\0', MAX_DATA_SIZE);
            //char msgmsg[MAX_DATA_SIZE];
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
            }
            if (strncmp(buf, "/", 1) == 0){
                state_status temp = clstates[index].state;
                clstates[index].state = get_cmd_type(buf);
                if (clstates[index].state == -1){
                    sprintf(msg, "Unknown command ");
                    printf("\n%s", msg);
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                }
                if (clstates[index].state == STATE_HELP){ 
                    sprintf(msg, "\nList of commands:\n/help\n/list - list of all available chatrooms\n/connect <to> - connect to chatroom\n/exit - exit chatroom\n/logout\n");
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = temp;
                    break;
                }
                if (clstates[index].state  == STATE_LIST){ 
                    sprintf(msg, "\nList of available rooms:");
                    /* SELECT * FROM users.db посмотреть список всех доступных комнат
                        if incorrect - esnd error msg, break;
                    */
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = temp;
                    break;
                }
            }
            else {
                sprintf(msg, "%d: %s", active_socket, buf);
                printf("%s", msg);
                for (int j = 0; j < MAX_CONNECT; j++){
                    if (clsockets[j] == active_socket){
                        continue;
                    }
                    else if (clsockets[j] == -1){
                        continue;
                    }
                    else {
                        if (send(clsockets[j], msg, MAX_DATA_SIZE-1, 0) == -1){
                            perror("Error. Server: send()");
                        }
                    }
                } 
            }
            break;
            /* End of case STATE_COMMONROOM */

        case STATE_CONNECT: 
            break;
    }
}

/*
void protocolClient(state_machine *clstates, char* buf, int sockfd) { // Chech in da documentation how to correctly write a star 
    switch (clstates->state){
        case STATE_LOGIN:
            printf("Write login: ");
            // buf was changed int the main code
            if(send(sockfd, buf, MAX_DATA_SIZE-1, 0) == -1) {
                perror("Error. Client: send");
                exit(1);
            }
            clstates->state = PASSWORD;
            break;
        case PASSWORD:
            break;
            
    }
} */