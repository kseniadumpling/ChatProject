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

typedef enum STATE {
  STATE_UNKNOWN = -1, STATE_START, STATE_LOGIN, STATE_PASSWORD, 
  STATE_SIGNUP, STATE_SIGNUP_PASS, STATE_COMMONROOM, STATE_HELP,
  STATE_LIST, STATE_CONNECT, STATE_LOGOUT, STATE_EXIT, STATE_CHATROOM
} state_status; 

typedef struct state_machine{
  state_status state;
  int sockfd;
} state_machine;


state_status get_cmd_type(char *cmd);
void protocol_server(int sendingSockfd, int *clsock, state_machine *states,
                 sqlite3 *db, char *buf, char *msg);
