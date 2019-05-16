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
        return STATE_CONNECT;

    if (strncmp(cmdargs, "/logout", strlen("/logout")) == 0)
        return STATE_LOGOUT;

    if (strncmp(cmdargs, "/quit", strlen("/quit")) == 0)
        return STATE_QUIT;

    return STATE_UNKNOWN;
}

int is_authorization_state(state_status clstate){ // CHECK CHECK CHECK !!!
    if (clstate == STATE_START || 
        clstate == STATE_LOGIN ||
        clstate == STATE_PASSWORD ||
        clstate == STATE_SIGNUP || 
        clstate == STATE_SIGNUP_PASS){
        return 0;
    }
    else {
        return 1;
    }  
}

void protocol_server(int active_socket, state_machine *clstates, sqlite3 *db_users, room *r_common, room *r_alpha) {

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
    switch (clstates[index].state[0]){

        case STATE_START:
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
            printf("sock %d: %s", active_socket, buf);
            
            // if msg is command
            if (!strncmp(buf, "/", 1)){
                clstates[index].state[1] = clstates[index].state[0]; // Saving old state
                clstates[index].state[0] = get_cmd_type(buf); // Get new state

                switch (clstates[index].state[0]){
                    case STATE_LOGIN:
                        sprintf(msg, "\nPlease, write login:");
                        if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                            perror("Error. Server: send()");
                        }
                        break;
                    
                    case STATE_SIGNUP:
                        sprintf(msg, "\nPlease, create new login:");
                        if(send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                            perror("Error. Server: send()");
                        }
                        break;

                    /*
                     *  TODO: add here /unknown
                     */
                }
            }
            // just regular msg
            else {
                printf("sock %d: undefined user.\n", active_socket);
                sprintf(msg, "\nPlease, sign up or log in");
                if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
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
             * TODO: Think about regular expression
             */
            
            printf("sock %d new login: %s", active_socket, buf);
            strncpy(clstates[index].login, buf, strlen(buf)-1);
      
            sprintf(msg, "Please, create password:\t");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
                break;
            }
            clstates[index].state[0] = STATE_SIGNUP_PASS;
            break;
            /* End of case STATE_SIGNUP */


        case STATE_SIGNUP_PASS:
        {
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
            printf("sock %d new password: %s", active_socket, buf);
           
            // Executing insertion to table
            char *err_msg;
            char sql[SQL_QUERY_SIZE];
            sprintf(sql, "INSERT INTO users (login, password) VALUES ('%s', '%s');", clstates[index].login, buf); 
            int handle = sqlite3_exec(db_users, sql, 0, 0, &err_msg); 
            if (handle != SQLITE_OK){
                fprintf(stderr, "SQL error: %s\n", err_msg);
                break;
            } 

            printf("New profile created successfully\n");
            sprintf(msg, "Success! Welcome to the common room\n");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
                break;
            }
            clstates[index].state[0] = STATE_COMMONROOM;
            r_common->sockets[r_common->num++] = active_socket;
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

            sprintf(msg, "Please, write password:\t");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Srever: send()");
                break;
            }
            clstates[index].state[0] = STATE_PASSWORD;
            break;
            /* End of case STATE_LOGIN */


        case STATE_PASSWORD:
        {
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                exit(1);
            }
            printf("sock %d password: %s", active_socket, buf); 
            
            // Executing acceptance of user
            char *sql;
            sqlite3_stmt *stmt;
            sprintf(sql, "SELECT * FROM users WHERE login = '%s ' AND password = '%s ';", clstates[index].login, buf); 
            sqlite3_prepare_v2(db_users, sql, -1, &stmt, NULL); 
            if(sqlite3_column_count(stmt) != 1){
                printf("Unknown profile\n");
                sprintf(msg, "\nWrong login or password\nPlease, sign up or log in");
                if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                    perror("Error. Server: send()");
                    break;
                }
                strcpy(clstates[index].login, "\0");
                clstates[index].state[0] = STATE_START;
                break;
            } 

            printf("%s logged in\n", clstates[index].login);
            sprintf(msg, "Correct.\nType /help to see all available commands\n");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
                break;
            } 
            clstates[index].state[0] = STATE_COMMONROOM;
            r_common->sockets[r_common->num++] = active_socket;
            break;
            /* End of case STATE_PASSWORD */
        }

        case STATE_COMMONROOM: 
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
            }
            // if msg is command
            if (strncmp(buf, "/", 1) == 0){
                clstates[index].state[1] = clstates[index].state[0]; // Saving old state
                clstates[index].state[0] = get_cmd_type(buf); // Get new state
            }
            // if it's just regular msg
            else {
                sprintf(msg, "%s: %s", clstates[index].login, buf);
                printf("%s", msg);

                for(int i = 0; i < r_common->num; i++){
                    if(r_common->sockets[i] == active_socket){
                        continue;
                    }
                    else if(r_common->sockets[i] == -1){
                        continue;
                    }
                    else {
                       // if (is_authorization_state(clstates[j].state[0]) != 0) {
                            if (send(r_common->sockets[i], msg, MAX_DATA_SIZE-1, 0) == -1){
                                perror("Error. Server: send()123");
                            } 
                       // }
                    }
                }

                /*
                for (int j = 0; j < MAX_CONNECT; j++){
                    if (clstates[j].sockfd == active_socket){
                        continue;
                    }
                    else if (clstates[j].sockfd == -1){
                        continue;
                    }
                    else {
                        
                       // if (is_authorization_state(clstates[j].state[0]) != 0) {
                            if (send(clstates[j].sockfd, msg, MAX_DATA_SIZE-1, 0) == -1){
                                perror("Error. Server: send()123");
                            } 
                       // }
                    }
                }  */
            }
            break;
            /* End of case STATE_COMMONROOM */

        /*
         * TODO: write scenario for other cases
         */
        case STATE_CONNECT:
            break;

        case STATE_CHATROOM:
            break;

        case STATE_QUIT:
            break;

        case STATE_LOGOUT:
            break;

        case STATE_UNKNOWN:
            printf("%d: Unknown command\n", active_socket);
            sprintf(msg, "\nUnknown command.");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
            }
            clstates[index].state[0] = clstates[index].state[1];
            clstates[index].state[1] = STATE_UNDEFINED;
            break;

        case STATE_HELP:
            sprintf(msg, "\nList of commands:\n/help - show this list\n" 
                        "/roomlist - list of all available chatrooms\n" 
                        "/connect <to> - connect to chatroom\n" 
                        "/quit - quit the chatroom\n" 
                        "/logout - change the profile\n");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
            }
            clstates[index].state[0] = clstates[index].state[1];
            clstates[index].state[1] = STATE_UNDEFINED;
            break;

        case STATE_ROOMLIST:
            sprintf(msg, "\nList of available rooms:"
                    "\n1. commonroom\n2. alpha\n3. beta\n4. gamma");
            /* 
            * TODO: Think about place, where info about room
            * will be held. Dunno, if it will be another structure
            * or another DB
            */
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
            }
            clstates[index].state[0] = clstates[index].state[1];
            clstates[index].state[1] = STATE_UNDEFINED;
            break;

        case STATE_UNDEFINED:
            printf("ERROR. Undefined state of %d", active_socket);
            sprintf(msg, "\nERROR. Undefined state. Restart client\n");
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
            }
            break;

    }
}
