/**
 * main.c
 *
 * Authors: Corbin Bremmeyr
 *          Richard Critchlow
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

#define READ 0
#define WRITE 1

#define MAX_NETWORK_SIZE 64
#define MAX_STR_LEN 256

typedef struct{
    pid_t dest;
    char data[MAX_STR_LEN];
} token_t;

int createNetwork(int len, pid_t *pids, int **fd);
int spawn(int len, pid_t *pids);
void fixPipeLeaks(pid_t *pids, int **fd, int len);

int getToken(int readFd, token_t *token);
int passToken(int writeFd, token_t *token);
int pidToIndex(pid_t *pids, int len);

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
    createNetwork(numComps, pids, fd);  // TODO check return for error

    // UI process
    if(pids[0] == getpid()) {

        token.dest = 3;
        strcpy(token.data, "hello there pdidy, how you doin' ;)");

        while(1) {
            const int pidIndex = 0;
            const int writeIndex = pidIndex;
            const int readIndex = numComps-1;

            passToken(fd[writeIndex][WRITE], &token);

            while(getToken(fd[readIndex][READ], &token) != 0);
        }
    }

    // Children processes
    else {
        const int pidIndex = pidToIndex(pids, numComps);
        const int writeIndex = pidIndex;
        const int readIndex = pidIndex-1;

        while(1) {

            // Wait to receive token
            while(getToken(fd[readIndex][READ], &token) != 0);

            // Process token
            if(token.dest == getpid()) {
                printf("%s", token.data);
            }

            // Give token to next node
            passToken(fd[writeIndex][WRITE], &token);
        }
    }

    free(pids);
    for(int i=0; i < numComps; i++) {
        free(fd[i]);
    }
    free(fd);

    return 0;
}

int pidToIndex(pid_t *pids, int len) {
    pid_t pid = getpid();
    for(int i=0; i < len; i++) {
        if(pids[i] == pid) {
            return i;
        }
    }
    return -1;
}

int createNetwork(int len, pid_t *pids, int **fd) {

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

    // Print to track token's path
    printf("PID = %d : getting token\t", getpid());
    printf("Token.dest = %d, .data = %s\n", token->dest, token->data);

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

    // Print to track token's path
    printf("PID = %d : passing token\t", getpid());
    printf("Token.dest = %d, .data = %s\n", token->dest, token->data);

    // Write to pipe
    size_t s = write(writeFd, (const void *) token, sizeof(token_t));

    // Check if data was written
    if(s != sizeof(token_t)) {
        return -1;
    }
    return 0;
}

// return 0 is good, -1 bad
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
                    printf("i = %d: close read %d\n", i, j);
                    close(fd[j][READ]);
                }

                // Close write side
                if(j != i) {
                    printf("i = %d: close write %d\n", i, j);
                    close(fd[j][WRITE]);
                }
            }
        }
    }
}

