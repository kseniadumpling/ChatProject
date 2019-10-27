#include "header.h"

state_status get_cmd_type(char *cmdargs) {
    if (strncmp(cmdargs, "/login", strlen("/login")) == 0)
        return STATE_LOGIN;

    if (strncmp(cmdargs, "/signup", strlen("/signup")) == 0)
        return STATE_SIGNUP;

    if (strncmp(cmdargs, "/help", strlen("/help")) == 0)
        return STATE_HELP;

    if (strncmp(cmdargs, "/roomlist", strlen("/roomlist")) == 0)
        return STATE_ROOMLIST;

    if (strncmp(cmdargs, "/connect", strlen("/connect")) == 0)
        return (get_room(cmdargs));

    if (strncmp(cmdargs, "/logout", strlen("/logout")) == 0)
        return STATE_LOGOUT;

    return STATE_UNKNOWN;
}

state_status get_room(char *cmd){
    char *temp;
    temp = strtok(cmd, " ");
    temp = strtok(NULL, " ");
    if(temp == NULL){
        return STATE_UNKNOWN;
    }
    if (strncmp(temp, "common", strlen("common")) == 0)
        return STATE_COMMONROOM;

    if (strncmp(temp, "alpha", strlen("alpha")) == 0)
        return STATE_ALPHA;

    if (strncmp(temp, "beta", strlen("beta")) == 0)
        return STATE_BETA;
}

void state_unknown_info(int socket){
    char msg[MAX_DATA_SIZE] = {"\0"};
    printf("%d: Unknown command\n", socket);
    sprintf(msg, "\nUnknown command.\n");
    if (write(socket, msg, strlen(msg)+1) == -1){
        perror("Error. Server: send()");
    }
}

void state_help_info(int socket){
    char msg[MAX_DATA_SIZE] = {"\0"};
    sprintf(msg, "\nList of commands:\n/help - show this list\n" 
                "/roomlist - list of all available chatrooms\n" 
                "/connect <to> - connect to chatroom\n" 
                "/logout - change the profile\n\n");
    if (write(socket, msg, strlen(msg)+1) == -1){
        perror("Error. Server: send()");
    }
}

void state_roomlist_info(int socket){
    char msg[MAX_DATA_SIZE] = {"\0"};
    sprintf(msg, "\nList of available rooms:"
            "\n1. common\n2. alpha\n\n");
    if (write(socket, msg, strlen(msg)+1) == -1){
        perror("Error. Server: send()");
    }
}

void state_wrongcmd_info(int socket){
    char msg[MAX_DATA_SIZE] = {"\0"};
    sprintf(msg, "\nThis command is not available\n");
    if (write(socket, msg, strlen(msg)+1) == -1){
        perror("Error. Server: send()");
    }
}

int is_authorization_state(state_status clstate){
    if (clstate == STATE_START || 
        clstate == STATE_LOGIN ||
        clstate == STATE_PASSWORD ||
        clstate == STATE_SIGNUP || 
        clstate == STATE_SIGNUP_PASS){
        return 0; // TODO: Add description
    }
    else {
        return 1;
    }  
}

void protocol_server(int active_socket, state_machine *clstates, sqlite3 *db_users, node **commonList, node **alphaList) {

    char buf[MAX_DATA_SIZE] = "\0";
    char msg[MAX_DATA_SIZE] = "\0";

    // Finding right index
    int index;
    for (index = 0; index < MAX_CONNECT; index++) {
        if (clstates[index].sockfd == active_socket){
            break;
        }
    }

    // Main logic: 
    switch (clstates[index].state){

        case STATE_START:
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
            printf("sock %d: %s", active_socket, buf);
            
            // if msg is command
            if (!strncmp(buf, "/", 1)){
                clstates[index].state = get_cmd_type(buf); // Get new state

                switch (clstates[index].state){
                    case STATE_LOGIN:
                        sprintf(msg, "\nPlease, write login: ");
                        if (write(active_socket, msg, strlen(msg)+1) == -1){
                            perror("Error. Server: send()");
                        }
                        break;
                    
                    case STATE_SIGNUP:
                        sprintf(msg, "\nPlease, create new login: ");
                        if(write(active_socket, msg, strlen(msg)+1) == -1){
                            perror("Error. Server: send()");
                        }
                        break;
                    
                    //all others command hide
                    default:
                        state_unknown_info(active_socket);
                        clstates[index].state = STATE_START;
                        break;
                }
            }
            // just regular msg
            else {
                printf("sock %d: undefined user.\n", active_socket);
                sprintf(msg, "\nPlease, sign up or log in\n");
                if (write(active_socket, msg, strlen(msg)+1) == -1){
                    perror("Error. Server: send()");
                    break;
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
             * TODO: Think about regular expression &
             *       checking the already used one.
             */
            printf("sock %d new login: %s", active_socket, buf);
            strncpy(clstates[index].login, buf, strlen(buf)-1);
      
            sprintf(msg, "Please, create password: ");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
                break;
            }
            clstates[index].state = STATE_SIGNUP_PASS;
            break;
            /* End of case STATE_SIGNUP */


        case STATE_SIGNUP_PASS:
        {
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
            printf("sock %d new password: %s", active_socket, buf);
            char pass[MAX_LOGIN_SIZE] = {"\0"}; // k o s t y l e
            strncpy(pass, buf, strlen(buf)-1);
           
            // Executing insertion to table
            char *err_msg;
            char sql[SQL_QUERY_SIZE];
            sprintf(sql, "INSERT INTO users (login, password) VALUES ('%s', '%s');", clstates[index].login, pass); 
            int handle = sqlite3_exec(db_users, sql, 0, 0, &err_msg); 
            if (handle != SQLITE_OK){
                fprintf(stderr, "SQL error: %s\n", err_msg);
                break;
            } 

            printf("New profile created successfully\n");
            sprintf(msg, "\nSuccess! \nWelcome to common room!\n" 
                         "Type /help to see all available commands\n\n");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
                break;
            }
            clstates[index].state = STATE_COMMONROOM;
            push(commonList, active_socket);
            break;
            /* End of case STATE_SIGNUP_PASS */
        }

        case STATE_LOGIN: 
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
            printf("sock %d login: %s", active_socket, buf); 
            strncpy(clstates[index].login, buf, strlen(buf)-1);

            sprintf(msg, "Please, write password: ");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Srever: send()");
                break;
            }
            clstates[index].state = STATE_PASSWORD;
            break;
            /* End of case STATE_LOGIN */


        case STATE_PASSWORD:
        {
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                exit(1);
            }
            printf("sock %d password: %s", active_socket, buf); 
            char pass[MAX_LOGIN_SIZE] = {"\0"}; // k o s t y l e
            strncpy(pass, buf, strlen(buf)-1);
            
            // Executing acceptance of user
            char sql[SQL_QUERY_SIZE];
            sqlite3_stmt *stmt;
            sprintf(sql, "SELECT * FROM users WHERE login = '%s' AND password = '%s';", clstates[index].login, pass); 
            sqlite3_prepare_v2(db_users, sql, -1, &stmt, NULL); 
            sqlite3_step(stmt);
            if(sqlite3_column_text(stmt, 1) == NULL){
                printf("Unknown profile\n");
                sprintf(msg, "\nWrong login or password\nPlease, sign up or log in\n");
                if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                    perror("Error. Server: send()");
                    break;
                }
                strcpy(clstates[index].login, "\0");
                clstates[index].state = STATE_START;
                sqlite3_finalize(stmt);
                break;
            } 
            sqlite3_finalize(stmt);

            printf("%s logged in\n", clstates[index].login);
            sprintf(msg, "\nCorrect.\nWelcome to common room!\n" 
                         "Type /help to see all available commands\n\n");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
                break;
            } 
            clstates[index].state = STATE_COMMONROOM;
            push(commonList, active_socket);
            break;
            /* End of case STATE_PASSWORD */
        }

        case STATE_COMMONROOM: 
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
            }
            // if msg is command
            if (strncmp(buf, "/", 1) == 0){
                clstates[index].state = get_cmd_type(buf); 
                //rewrite as a function
                switch (clstates[index].state){
                case STATE_HELP:
                    state_help_info(active_socket);
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                case STATE_ROOMLIST:
                    state_roomlist_info(active_socket);
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                case STATE_UNKNOWN:
                    state_unknown_info(active_socket);
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                case STATE_ALPHA:
                    sprintf(msg, "\nWelcome to alpha room\n");
                    if (write(active_socket, msg, strlen(msg)+1) == -1){
                        perror("Error. Server: send()");
                    }
                    delete_node(commonList, active_socket);
                    push(alphaList, active_socket);
                    break;
                // Rewrite this. Think about architecture
                case STATE_LOGOUT:
                    sprintf(msg, "\nYou logged out.\nType /signup or /login to continue...\n");
                    if (write(active_socket, msg, strlen(msg)+1) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_START;
                    memset(clstates[index].login, '\0', MAX_LOGIN_SIZE); 
                    delete_node(commonList, active_socket);
                    break;
                    
                default:
                    if(is_authorization_state(clstates[index].state) == 0){
                        state_wrongcmd_info(active_socket);
                        clstates[index].state = STATE_COMMONROOM;
                    }
                    break;
                }
            }
            // if it's just regular msg
            else {
                sprintf(msg, "%s: %s", clstates[index].login, buf);
                printf("%s", msg);                
                struct Node *cursor = *commonList;
                while (cursor != NULL){
                    if (cursor->data == active_socket){
                        cursor = cursor->next;
                        continue;
                    }
                    else {
                        if (send(cursor->data, msg, MAX_DATA_SIZE-1, 0) == -1){
                            perror("Error. Server: send()123");
                        } 
                    }
                    cursor = cursor->next;
                }
            }
            break;
            /* End of case STATE_COMMONROOM */

        case STATE_ALPHA: 
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
            }
            // if msg is command
            if (strncmp(buf, "/", 1) == 0){
                clstates[index].state = get_cmd_type(buf); 
                //rewrite as a function
                switch (clstates[index].state){
                case STATE_HELP:
                    state_help_info(active_socket);
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                case STATE_ROOMLIST:
                    state_roomlist_info(active_socket);
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                case STATE_UNKNOWN:
                    state_unknown_info(active_socket);
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                case STATE_COMMONROOM:
                    sprintf(msg, "\nWelcome to common room\n");
                    if (write(active_socket, msg, strlen(msg)+1) == -1){
                        perror("Error. Server: send()");
                    }
                    delete_node(alphaList, active_socket);
                    push(commonList, active_socket);
                    break;
                // Rewrite this. Think about architecture
                case STATE_LOGOUT:
                    sprintf(msg, "\nYou logged out.\nType /signup or /login to continue...\n");
                    if (write(active_socket, msg, strlen(msg)+1) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_START;
                    memset(clstates[index].login, '\0', MAX_LOGIN_SIZE); 
                    delete_node(alphaList, active_socket);
                    break;

                default:
                    if(is_authorization_state(clstates[index].state) == 0){
                        state_wrongcmd_info(active_socket);
                        clstates[index].state = STATE_COMMONROOM;
                    }
                    break;
                }
            }
            // if it's just regular msg
            else {
                sprintf(msg, "alpha|%s: %s", clstates[index].login, buf);
                printf("%s", msg);                
                struct Node *cursor = *alphaList;
                while (cursor != NULL){
                    if (cursor->data == active_socket){
                        cursor = cursor->next;
                        continue;
                    }
                    else {
                        if (send(cursor->data, msg, MAX_DATA_SIZE-1, 0) == -1){
                            perror("Error. Server: send()123");
                        } 
                    }
                    cursor = cursor->next;
                }
            }
            break;
            /* End of case STATE_ALPHA */

        case STATE_LOGOUT:
            break;
    }
}
