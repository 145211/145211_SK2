
// Based on:
// server.c
// Version 20161003
// Written by Harry Wong (RedAndBlueEraser)
//
// Multithreaded server for tic tac toe game
// Written by 145211

#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>

#define BACKLOG 10
#define BUFSIZE 2

typedef struct pthread_arg_t {
    int new_socket_fd1;
    int new_socket_fd2;
    struct sockaddr_in client_address;

    int sck;
    pthread_mutex_t lock;

    char (*games)[10];
    int gameNum;
} pthread_arg_t;

// thread routine to serve connection to client
void *pthread_routine(void *arg);

// Signal handler to handle SIGTERM signal.
void signal_handler(int signal_number);

// checking the state of the game
char gameCheck(char state[10]);

// Global socket id for ease of closing
int socket_fd;

int main(int argc, char *argv[]) {
    int port, new_socket_x, new_socket_o;
    struct sockaddr_in address;
    pthread_attr_t pthread_attr;
    pthread_arg_t *pthread_argx;
    pthread_arg_t *pthread_argo;
    pthread_t pthread;
    socklen_t client_address_len;

    // Get port from command line arguments or stdin. 
    port = argc > 1 ? atoi(argv[1]) : 0;
    if (!port) {
        printf("Enter Port: ");
        scanf("%d", &port);
    }

    // Initialise IPv4 address. 
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    // Create TCP socket. 
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    // Bind address to socket. 
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof address) == -1) {
        perror("bind");
        exit(1);
    }

    // Listen on socket. 
    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    // Assign signal handler to signals. 
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    } 

    // Initialise pthread attribute to create detached threads. 
    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("pthread_attr_init");
        exit(1);
    }
    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("pthread_attr_setdetachstate");
        exit(1);
    }

    // counter for x/o players
    int even = 1;
    // global games table
    char games[256][10];
    // game iterator
    int gameNum = 0;

    while (1) {
        // Create pthread arguments for each pair of connections to client. 
        if (even) {
            pthread_argx = (pthread_arg_t *)malloc(sizeof *pthread_argx);
            pthread_argo = (pthread_arg_t *)malloc(sizeof *pthread_argo);
            strcpy(games[gameNum], "nnnnnnnnn");
            // mutex for accessing games table
            pthread_mutex_t lock;
            pthread_mutex_init(&lock, NULL);

            pthread_argx->lock = lock; 
            pthread_argx->games = games; 
            pthread_argx->gameNum = gameNum; 

            pthread_argo->lock = lock; 
            pthread_argo->games = games; 
            pthread_argo->gameNum = gameNum; 

            gameNum++;
        }
        if (!pthread_argx || !pthread_argo) {
            perror("malloc");
            continue;
        }

        // reseting the counter
        if (gameNum == 255) { 
            gameNum = 0;
        }

        // Accept connection to client. 
        client_address_len = sizeof pthread_argx->client_address;

        // if even, accept connection and await another player
        if (even) {
            new_socket_x = accept(socket_fd, (struct sockaddr *)&pthread_argx->client_address, &client_address_len);
            if (new_socket_x == -1) {
                perror("accept");
                free(pthread_argx);
                continue;
            }
            
            pthread_argx->sck = even;
            even = 0;
        }
        // if odd accept the second connection and create threads
        else {
            new_socket_o = accept(socket_fd, (struct sockaddr *)&pthread_argo->client_address, &client_address_len);
            if (new_socket_o == -1) {
                perror("accept");
                free(pthread_argo);
                continue;
            }

            // pass socket ids each thread
            pthread_argx->new_socket_fd1 = new_socket_x;
            pthread_argx->new_socket_fd2 = new_socket_o;
            pthread_argo->new_socket_fd1 = new_socket_x;
            pthread_argo->new_socket_fd2 = new_socket_o;

            pthread_argo->sck = even; 
            even = 1;

            // create thread to serve connection to client x
            if (pthread_create(&pthread, &pthread_attr, pthread_routine, (void *)pthread_argx) != 0) {
                perror("pthread_create");
                free(pthread_argx);
                continue;
            }

            // create thread to serve connection to client o
            if (pthread_create(&pthread, &pthread_attr, pthread_routine, (void *)pthread_argo) != 0) {
                perror("pthread_create");
                free(pthread_argo);
                continue;
            }
        }
    }
    return 0;
}

// thread routine to serve connection to client
void *pthread_routine(void *arg) {
    // localazing required shared data
    pthread_arg_t *pthread_arg = (pthread_arg_t *)arg;
    struct sockaddr_in client_address = pthread_arg->client_address;
    int new_socket_fd;
    int new_socket_opp;

    pthread_mutex_t lock = pthread_arg->lock;
    char (*games)[10] = pthread_arg->games;
    int gameNum = pthread_arg->gameNum;
    char player;
    char winner;
    char prev;

    // set socket id depending on player's symbol
    if (pthread_arg->sck == 1) {
        new_socket_fd = pthread_arg->new_socket_fd1;
        new_socket_opp = pthread_arg->new_socket_fd2;
        player = 'x';
    }
    else {
        new_socket_fd = pthread_arg->new_socket_fd2;
        new_socket_opp = pthread_arg->new_socket_fd1;
        player = 'o';
        free(arg);
    }        

    // create communication buffers
    char msgFr[BUFSIZE];
    char msgTo[BUFSIZE];  

    // inform clients about their symbols 
    msgTo[0] = player;
    msgTo[1] = '\n';
    write(new_socket_fd, &msgTo, BUFSIZE);

    // while the connection exists
    while (read(new_socket_fd, &msgFr, BUFSIZE)) {

        // pass data between players
        if (player == 'x') {
            if (msgFr[0] - '0' < 9) {
                pthread_mutex_lock(&lock);
                games[gameNum][msgFr[0] - '0'] = player;
                pthread_mutex_unlock(&lock);
            }
            msgTo[0] = msgFr[0];
            write(new_socket_opp, &msgTo, BUFSIZE);
        }

        if (player == 'o') {
            if (msgFr[0] - '0' < 9) {
                pthread_mutex_lock(&lock);
                games[gameNum][msgFr[0] - '0'] = player;
                pthread_mutex_unlock(&lock);
            }
            msgTo[0] = msgFr[0];
            write(new_socket_opp, &msgTo, BUFSIZE);
        }

        // check the state of the game
        winner = gameCheck(games[gameNum]);

        // inform the clients about the state of the game
        if (winner != 'n') {\
            msgTo[0] = winner;
            write(new_socket_opp, &msgTo, BUFSIZE);
        }
        else if (winner == 'd') {
            msgTo[0] = 'd';
            write(new_socket_fd, &msgTo, BUFSIZE);
        }
        else {
            msgTo[0] = 'n';
            write(new_socket_opp, &msgTo, BUFSIZE);
        }

        // endless loop breaker
        if (prev == msgFr[0]){
            break;
        }
        prev = msgFr[0];
    }

    // closing connections
    close(new_socket_fd);
    close(new_socket_opp);
    return NULL;
}

// close connection on termination
void signal_handler(int signal_number) {
    close(socket_fd);
    exit(0);
}

// checking the state of the game
// x - xwins, o - o wins, d - draw, n - not resolved
char gameCheck(char state[10]) {
    // winning positions
    int wins[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},
        {0,3,6}, {1,4,7}, {2,5,8},
        {0,4,8}, {2,4,6}};

    // curren positions
    int inx[5] = {9, 9, 9, 9, 9};
    int ino[5] = {9, 9, 9, 9, 9};
    int no = 0;
    int nx = 0;

    // check current positions
    for (int i = 0; i < 9; i++) {
        if (state[i] == 'x') {
            inx[nx] = i;
            nx++;
        } 
        else if (state[i] == 'o') {
            ino[no] = i;
            no++;
        }
    }

    // check if any wins
    int matchx;
    int matcho;
    // loop over win conditions
    for (int i = 0; i < 8; i++) {
        matchx = 0;
        matcho = 0;
        // loop over win condition positions
        for (int j = 0; j < 3; j++) {
            // loop over current positions
            for (int k = 0; k < 5; k++) {
                // if a position matches with a win condition increase the counter
                if (inx[k] == wins[i][j]) {
                    matchx++;
                }
                else if (ino[k] == wins[i][j]) {
                    matcho++;
                }
            }
            // 3 matches mean the current position matches a win condition
            if (matchx == 3) {
                // x wins
                return 'x';
            }
            else if (matcho == 3) {
                // o wins
                return 'o';
            }
        }
    }
    // draw
    if (nx + no == 9) {
        return 'd';
    }

    return 'n';
}