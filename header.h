#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>

#include <netdb.h>
#include <sqlite3.h>


#define PORT "7000"
#define MAX_EVENTS 1024
#define LISTENQ 100
#define MAX_CONNECT 100
#define MAX_DATA_SIZE 256
#define MAX_LOGIN_SIZE 32
#define SQL_QUERY_SIZE 1024

typedef struct Node { 
  int data; 
  struct Node *next; 
} node;

void push(node **head_ref, int new_data) ;
void append(node **head_ref, int new_data);
void print_list(node *node);
void delete_node(node **head_ref, int key);

typedef enum STATE { 
  STATE_UNKNOWN = -1, STATE_START, STATE_LOGIN, STATE_PASSWORD, 
  STATE_SIGNUP, STATE_SIGNUP_PASS, STATE_HELP, STATE_ROOMLIST,
  STATE_LOGOUT, STATE_COMMONROOM, STATE_ALPHA, STATE_BETA
} state_status; 

typedef struct state_machine {
  state_status state;
  int sockfd;
  char login[MAX_LOGIN_SIZE];
} state_machine;

state_status get_cmd_type(char *cmd);
state_status get_room(char *cmd);
void protocol_server(int sendingSockfd, state_machine *states, sqlite3 *db,
                      node **commonList, node **alphaList);
int is_authorization_state(state_status clstate);
int execute_db(sqlite3 *db_ptr, char *name, char *sql); 

