/**
 * main.c
 *
 * Authors: Corbin Bremmeyr
 *          Richard Critchlow
 * Date: 13 February 2021
 *
 * Entry point for CIS 452 project 1
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>

#define READ 0
#define WRITE 1

#define MAX_NETWORK_SIZE 64
#define MAX_STR_LEN 256

typedef struct{
    pid_t dest;
    char data[MAX_STR_LEN];
} token_t;

typedef struct {
    int nodeCount;
    int dest;
    char str[MAX_STR_LEN];
} userInput_t;

typedef enum TKN_STAT{EMPTY, FULL, RETURNING} tkn_stat_t;

int createNetwork(int len, pid_t *pids, int **fd);
int spawn(int len, pid_t *pids);
void fixPipeLeaks(pid_t *pids, int **fd, int len);

tkn_stat_t checkToken(token_t *token);
int getToken(int readFd, token_t *token);
int getTokenNB(int readFd, token_t *token);
int passToken(int writeFd, token_t *token);
int pidToIndex(pid_t *pids, int len);

void *getUserInput(void *arg);

void sigHandler(int sig);

volatile bool userInputDone;

int main(int argc, char* args[]){

    int **fd = NULL;
    int numComps = 0;
    pid_t *pids = NULL;
    token_t token = {0};

    // Check if user gave at least one comand line argument
    if(argc < 2) {
        printf("Requires at least one argument\n");
        exit(1);
    }

    // Convert user input to an int
    numComps = atoi(args[1]);

    // Check if input is too big
    if(numComps > MAX_NETWORK_SIZE) {
        printf("Input too big, using %d instead\n", MAX_NETWORK_SIZE);
        numComps = MAX_NETWORK_SIZE;
    }

    // Check if input is too small/negative
    if(numComps <= 1) {
        printf("Need at least 2 computers for a network\n");
        exit(1);
    }

    // Make network
    pids = malloc(numComps * sizeof(pid_t));
    if(pids == NULL) {
        printf("Error: out of memory\n");
        exit(1);
    }

    // Make 2-D array of file descriptors
    fd = (int **) malloc(numComps * sizeof(int *));
    if(fd == NULL) {
        printf("Error: out of memory\n");
        exit(1);
    }
    for(int i=0; i < numComps; i++) {
        fd[i] = malloc(2 * sizeof(int));
        if(fd[i] == NULL) {
            printf("Error: out of memory\n");
            exit(1);
        }
    }

    // Make network
    if(createNetwork(numComps, pids, fd) == -1) {
        printf("Failed to create network\n");
        exit(1);
    }

    // UI process (parent)
    if(pids[0] == getpid()) {

        const int pidIndex = 0;
        const int writeIndex = pidIndex;
        const int readIndex = numComps-1;
        bool threadSpawned = false;
        userInputDone = false;

        pthread_t thread;
        int threadStatus;
        userInput_t userInput = {0};
        userInput.nodeCount = numComps;
        userInput.dest = -1;

        bool haveData = false;
        bool waitForReply = false;

        while(1) {

            // Try to get token
            // If token was recived
            if(getTokenNB(fd[readIndex][READ], &token) == 0) {

                printf("Node %d: recived token -- ", pidIndex);
                printf("token.dest = %d, .data = %s\n", token.dest, token.data);

                switch(checkToken(&token)){
                    case RETURNING:
                        if(waitForReply) {
                            strcpy(token.data, "");
                        }
                        break;

                    case FULL:
                        if(token.dest == pidIndex){
                            printf("PID = %d : This is for me\n\t\"%s\"\n", pidIndex, token.data);
                            token.dest = 0;
                        }
                        break;

                    case EMPTY:
                        break;
                }

                printf("\tPassing token -- ");
                printf("token.dest = %d, .data = %s\n", token.dest, token.data);

                passToken(fd[writeIndex][WRITE], &token);
            }

            // If user input is needed
            if(!haveData) {

                // If a user input thread has been created
                if(threadSpawned) {

                    // If thread has finsihed
                    if(userInputDone) {
                        pthread_join(thread, NULL);
                        threadSpawned = false;
                        haveData = true;
                        userInputDone = false;

                        token.dest = userInput.dest;
                        strcpy(token.data, userInput.str);

                        passToken(fd[writeIndex][WRITE], &token);
                        waitForReply = true;
                    }
                }
                else {

                    // Spawn user input thread
                    if((threadStatus = pthread_create(&thread, NULL, getUserInput, (void *) &userInput)) != 0) {
                        fprintf(stderr, "Thread create error %d: %s\n", threadStatus, strerror(threadStatus));
                        exit(1);
                    }
                    threadSpawned = true;
                }
            }

        }
    }

    // Children processes
    else {
        const int pidIndex = pidToIndex(pids, numComps);
        const int writeIndex = pidIndex;
        const int readIndex = pidIndex-1;
        bool waitForReply = false;

        while(1) {

            // Wait to receive token
            if(getToken(fd[readIndex][READ], &token) == 0) {

                printf("Node %d: recived token -- ", pidIndex);
                printf("token.dest = %d, .data = %s\n", token.dest, token.data);

                switch(checkToken(&token)){
                    case RETURNING:
                        if(waitForReply) {
                            strcpy(token.data, "");
                        }
                        break;

                    case FULL:
                        if(token.dest == pidIndex){
                            printf("PID = %d : This is for me\n\t\"%s\"\n", pidIndex, token.data);
                            token.dest = 0;
                        }
                        break;

                    default:
                        break;
                }

                printf("\tPassing token -- ");
                printf("token.dest = %d, .data = %s\n", token.dest, token.data);

                passToken(fd[writeIndex][WRITE], &token);
            }
        }
    }

    return 0;
}

/**
 * Thread funciton to get input from user
 *
 * param *arg - pointer to userInput_t to hold data passed between thread and parent
 *
 * return NULL (needed to work with pthreads)
 */
void *getUserInput(void *arg) {
    userInput_t *userInput = (userInput_t *) arg;
    char tempStr[MAX_STR_LEN] = {0};

    // Prompt user for node to send message to
    do {
        printf("Enter destination node (1 to %d): ", userInput->nodeCount-1);
        fgets(tempStr, MAX_STR_LEN, stdin);
        userInput->dest = atoi(tempStr);
    } while(userInput->dest < 1 || userInput->dest > userInput->nodeCount-1);

    // Promp user for messsage to send
    printf("Enter message to send (max %d chars):\n", MAX_STR_LEN);
    fgets(userInput->str, MAX_STR_LEN, stdin);

    // Set falg to show thread has finished
    userInputDone = true;

    return NULL;
}

/**
 * Check if token is being sent to this desination.
 *
 * param *token - pointer to token to check
 *
 * return status of token. Indecates how the node should handle the
 * token before passing.
 */
tkn_stat_t checkToken(token_t *token) {

    // If token is being sent to parent node
    if(token->dest == 0) {

        // If token doesn't have any data
        if(strlen(token->data) == 0) {

            // Token is empty and should be passed w/o doing anything
            return EMPTY;
        }

        // Token is going back to parent node
        return RETURNING;
    }

    // Token has data and is going to destination node
    return FULL;
}

/**
 * Convert actual PID to virtual PID. This is done by searching the given PID table.
 * Searching is done in a linear fashion, O(len).
 *
 * param *pids - table of PIDs to search
 * param len - number of entries in PID table
 *
 * return processes virtual PID used by the simulation, -1 if not found
 */
int pidToIndex(pid_t *pids, int len) {
    pid_t pid = getpid();
    for(int i=0; i < len; i++) {

        // If PID is found in table
        if(pids[i] == pid) {

            // Return the virtual PID
            return i;
        }
    }

    // If PID was not in table
    return -1;
}

/**
 * Creates child processes and plumps them to create simulated token-ring network
 *
 * param len - number of processes in network
 * param *pids - table of process PIDs
 * param **fd - 2D array of pipe file descriptors
 *
 * return 0 on success, -1 on fail
 */
int createNetwork(int len, pid_t *pids, int **fd) {

    // Need at least 2 computers to make a network
    if(len < 2) {
        printf("A network must have at least two computers\n");
        return -1;
    }

    // Make all needed pipes
    for(int i=0; i < len; i++) {
        pipe(fd[i]);
    }

    // Put parent PID in pid table
    pids[0] = getpid();

    // Make children processes
    spawn(len-1, &pids[1]);

    // Make network out of pipes
    fixPipeLeaks(pids, fd, len);

    // Set SIGINT handler for all processes
    signal(SIGINT, sigHandler);

    return 0;
}

/**
 * Read token from pipe (blocking)
 *
 * param readFd - pipe file descriptor to read from
 * param *token - location to store token recived from pipe
 *
 * return 0 on success, -1 if not all data was passed
 */
int getToken(int readFd, token_t *token) {

    // Read from pipe
    ssize_t s = read(readFd, (void *) token, sizeof(token_t));
    if(s != sizeof(token_t)) {
        return -1;
    }

    return 0;
}

/**
 * Read token from pipe (non-blocking)
 *
 * param readFd - pipe file descriptor to read from
 * param *token - location to store token recived from pipe
 *
 * return 0 on success, -1 if no data was read
 */
int getTokenNB(int readFd, token_t *token) {

    // Save original flags
    int flags = fcntl(readFd, F_GETFL, 0);

    // Set pipe reading to be non-blocking
    fcntl(readFd, F_SETFL, flags | O_NONBLOCK);

    // Read from pipe
    ssize_t s = read(readFd, (void *) token, sizeof(token_t));

    // If no data was read
    if(s <= 0) {

        // Restore original pipe flags
        fcntl(readFd, F_SETFL, flags);
        return -1;
    }

    // If only some of the data was read
    else if(s < sizeof(token_t)) {

        // Restore original pipe flags
        fcntl(readFd, F_SETFL, flags);

        // Read rest of data
        char *temp = ((char *) token) + s;
        read(readFd, (void *) temp, sizeof(token_t)-s);
        return 0;
    }

    // Restore original pipe flags
    fcntl(readFd, F_SETFL, flags);
    return 0;
}

/**
 * Pass token to next node in ring
 *
 * param writeFd - pipe file descriptor to read from
 * param *token - location of token to write to pipe
 *
 * return 0 on success, -1 if not all data was passed
 */
int passToken(int writeFd, token_t *token) {

    sleep(1);

    // Write to pipe
    size_t s = write(writeFd, (const void *) token, sizeof(token_t));

    // Check if data was written
    if(s != sizeof(token_t)) {
        return -1;
    }
    return 0;
}

/**
 *  Spawns given number of child processes in a linear decendency
 *
 *  param len - number of child processes to make
 *  param *pids - table to be filled that maps each PID to a virtual PID (index),
 *          pids must point to at least len*sizeof(pid_t) bytes
 *
 * return 0 if successful, -1 if failure
 */
int spawn(int len, pid_t *pids) {

    if(len > 0) {

        pid_t pid = fork();

        // Fork error checking
        if(pid < 0) {
            return -1;
        }

        // If child process
        if(pid == 0) {
            pids[0] = getpid();
            return spawn(len-1, &(pids[1]));
        }
    }

    return 0;
}

/**
 * Closes pipes that are inherrited by processes that the process should
 * not interact with.
 *
 * param *pids - table of virtual PIDs to accual PID
 * param **fd - 2D array of file descriptors for pipes
 * param len - Number of processes in simulated token-ring network
 */
void fixPipeLeaks(pid_t *pids, int **fd, int len) {

    // For each process close all un-needed pipes
    for(int i=0; i < len; i++) {
        if(pids[i] == getpid()){

            // fd[i-1] is pipe to read from (not for OG parent)
            // fd[i] is pipe to write to
            // close rest of pipes
            for(int j=0; j < len; j++) {

                int readIndex = i-1;
                if(readIndex < 0) {
                    readIndex = len-1;
                }

                // Close read side
                if(j != readIndex) {
                    close(fd[j][READ]);
                }

                // Close write side
                if(j != i) {
                    close(fd[j][WRITE]);
                }
            }
        }
    }
}

/**
 *  Have process exit on SIGINT
 *
 *  param sig - unused
 */
void sigHandler(int sig) {
    printf("PID = %d: goodbye\n", getpid());
    exit(0);
}

