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

/*
 * TODO: Fix it.
 */
int execute_db(sqlite3 *db_ptr, char *name, char *sql){
    // Opening database
    char *err_msg = 0;
    int handle = sqlite3_open(name, &db_ptr);
    if (handle != SQLITE_OK){
        printf("Cannot open database: %s\n", sqlite3_errmsg(db_ptr));
        sqlite3_close(db_ptr);
        return -1;
    }
    // Executing
    handle = sqlite3_exec(db_ptr, sql, 0, 0, &err_msg); 
    if (handle != SQLITE_OK){
        printf("SQL error: %s\n", err_msg);
        sqlite3_close(db_ptr);
        return -1;
    }

    // Closing.
    sqlite3_close(db_ptr);
    
    return 0;
}


void protocol_server(int active_socket, int *clsockets, state_machine *clstates,
                       sqlite3 *db_users, char *buf, char *msg) {

    memset(buf, '\0', MAX_DATA_SIZE);
    memset(msg, '\0', MAX_DATA_SIZE);

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
            printf("%d: %s", active_socket, buf);
            
            // if msg is command
            if (strncmp(buf, "/", 1) == 0){
                clstates[index].state = get_cmd_type(buf);
                // Unknown cmd
                if(clstates[index].state == -1){
                    printf("%d: Unknown command\n", active_socket);
                    sprintf(msg, "Unknown command. Please, type /signup or /login\t");
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_START;
                }
                // Login cmd
                else if (clstates[index].state  == STATE_LOGIN){ 
                    sprintf(msg, "\nPlease, write login:\t");
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_LOGIN;
                }
                // Sign up cmd
                else if(clstates[index].state  == STATE_SIGNUP){
                    sprintf(msg, "\nPlease, create new login:\t");
                    if(send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_SIGNUP;
                }
            }
            // just regular msg
            else {
                printf("%d: undefined user.\n", active_socket);
                sprintf(msg, "Please, sign up or log in\t");
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
            printf("New login %d: %s", active_socket, buf);
            strcpy(clstates[index].login, buf);
       
      
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
            printf("New password %d: %s", active_socket, buf);

            // Executing insertion to table

            /*  Here goes segmentation fault. 
                Check sqlite.org/cintro.html, 
                mb answer will be there

            char *sql;
            sprintf(sql, "INSERT INTO users (login, password) VALUES ('%s', 'undefine');", buf); 
            if (execute_db(db_users, "users.db", sql) == -1){
                printf("Error. Insert to database users.db");
                break;
            } 
            */

            // TODO: rewrite, using func
            char *err_msg;
            char *sql;
            printf("Checking: %s", clstates[index].login);
            sprintf(sql, "INSERT INTO users (login, password) VALUES ('%s', '%s');", clstates[index].login, buf) ; 
            int handle = sqlite3_exec(db_users, sql, 0, 0, &err_msg); 
            if (handle != SQLITE_OK){
                fprintf(stderr, "SQL error: %s\n", err_msg);
                sqlite3_close(db_users);
                break;
            }
            
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
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
                break;
            }
            printf("Login %d: %s", active_socket, buf); 
            strcpy(clstates[index].login, buf);

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
            printf("Password %d: %s", active_socket, buf); 
            /* SELECT * FROM users.db... compare pass and login
                if incorrect - send error msg, break;
            */
            clstates[index].state = STATE_COMMONROOM;

            sprintf(msg, "\nCorrect.\nType /help to see all available commands\n"); 
            if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: send()");
            }
            break;
            /* End of case STATE_PASSWORD */

        case STATE_COMMONROOM: 
            if (recv(active_socket, buf, MAX_DATA_SIZE-1, 0) == -1){
                perror("Error. Server: recv()");
            }
            // if msg is command
            if (strncmp(buf, "/", 1) == 0){
                clstates[index].state = get_cmd_type(buf);

                if (clstates[index].state == -1){
                    sprintf(msg, "Unknown command ");
                    printf("%d: %s\n", active_socket, msg);
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                }

                if (clstates[index].state == STATE_HELP){ 
                    sprintf(msg, "\nList of commands:\n/help\n" 
                                 "/list - list of all available chatrooms\n" 
                                 "/connect <to> - connect to chatroom\n" 
                                 "/exit - exit chatroom\n" 
                                 "/logout\n");
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                }

                if (clstates[index].state  == STATE_LIST){ 
                    sprintf(msg, "\nList of available rooms:");
                    /* 
                     * TODO: Think about place, where info about room
                     * will be held. Dunno, if it will be another structure
                     * or another DB
                     */
                    if (send(active_socket, msg, MAX_DATA_SIZE-1, 0) == -1){
                        perror("Error. Server: send()");
                    }
                    clstates[index].state = STATE_COMMONROOM;
                    break;
                }
            }
            // if it's just regular msg
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
                        // definitely should be rewritten
                        if (clstates[j].state != STATE_START && 
                            clstates[j].state != STATE_LOGIN &&
                            clstates[j].state != STATE_PASSWORD &&
                            clstates[j].state != STATE_SIGNUP && 
                            clstates[j].state != STATE_SIGNUP_PASS) {
                            if (send(clsockets[j], msg, MAX_DATA_SIZE-1, 0) == -1){
                                perror("Error. Server: send()");
                            }
                        }
                    }
                } 
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

        case STATE_EXIT:
            break;

        case STATE_LOGOUT:
            break;

    }
}
