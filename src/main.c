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
    int dest;
    char data[MAX_STR_LEN];
} token_t;

int createNetwork(int n, pid_t *pids, int *fd);
int createNetworkHelper(int n, pid_t *pids, int *fd);
int getToken(int readFd, token_t *token);
int passToken(int writeFd, token_t *token);

int main(int argc, char* args[]){

    int fd[2];
    int ret = 0;
    int numComps = 0;
    pid_t *pids = NULL;

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
    ret = createNetwork(numComps, pids, fd);


    free(pids);

    return 0;
}

int createNetwork(int n, pid_t *pids, int *fd) {

    // If invalid input
    if(n < 1 || pids == NULL) {
        return -1;
    }

    // Make last pipe in ring
    if(pipe(fd) < 0) {
        return -1;
    }

    // Close unused end of pipe
    close(fd[WRITE]);

    // Make most of network
    if(createNetworkHelper(n, pids, fd)) {
        return -1;
    }

    return 0;
}

/************************************************************
 * Creates a processes that represents a new computer recursively
 * A value of n <= 1 will result in no additional processes being created.
 *
 ***********************************************************/
int createNetworkHelper(int n, pid_t *pids, int *fd) {

    pid_t curPid;
    pids[0] = getpid();

    printf("Computer %d: %d\n", n, pids[0]);

    if(n-1 > 0) {

        // Make pipes
        if(pipe(fd) < 0) {
            return -1;
        }

        curPid = fork();

        // Fork error
        if(curPid < 0) {
            return -1;
        }

        // Child process
        else if (curPid == 0) {

            // Close unused file descriptors
            close(fd[WRITE]);
            close(stdin);

            return createNetwork(n-1, &pids[1]);
        }

        // Parent process
        else {

            // Close unused file (read) side of the pipe
            close(fd[READ]);
        }
    }

    return 0;
}

/**
 * Read token from pipe (blocking)
 *
 * return 0 on success, -1 if not all data was passed
 */
int getToken(int readFd, token_t *token) {

    // Read from pipe
    ssize_t s = read(readFd, (void *) token, sizeof(token));
    if(s != sizeof(token)) {
        return -1;
    }

    // Print to track token's path
    printf("PID = %d : recived token\n", getpid());

    return 0;
}

/**
 * Pass token to next node in ring
 *
 * return 0 on success, -1 if not all data was passed
 */
int passToken(int writeFd, token_t *token) {

    sleep(1);

    // Print to track token's path
    printf("PID = %d : passing token\n", getpid());

    // Write to pipe
    size_t s = write(writeFd, (const void *) token, sizeof(token));

    // Check if data was writen
    if(s != sizeof(token)) {
        return -1;
    }
    return 0;
}

