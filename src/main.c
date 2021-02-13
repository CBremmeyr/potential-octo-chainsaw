#include "stdio.h"


typedef struct{
    int header;
    int data[256];
}Token;

int main(int arg, char* args[]){

    int ret;
    
    ret = createComputers(NUM_COMPUTERS); 



    return 0;
}

/************************************************************
 * Creates a processes that represents a new computer recursively
 * A value of n <= 1 will result in no additional processes being created. 
 *
 ***********************************************************/
int createComputers(int n){

    int err = 0;
    pid_t pid;

    if((n-1 > 0) && !err){        
        pid = fork();
        if(pid < 0){
            perror("fork failure\n");
            err = -1;   
        }   
        else if (pid == 0){{
            err = createComputers(--n);
        }
    }

    return err;
}
