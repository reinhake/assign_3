#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

volatile sig_atomic_t gSignalStatus = 0;
volatile sig_atomic_t gBackgroundStatus = 0;


void handle_SIGINT(int signo){
    char *mesg = "terminated by signal %d\n";
    char msg[23];
    sprintf(msg, mesg, signo); 
    write(STDOUT_FILENO, msg, 23);
    gSignalStatus = signo;
}

void handle_SIGTSTP(int signo){
    if (gBackgroundStatus == 0){
        gBackgroundStatus = 1;
        char *mesg = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, mesg, 50);
    }
    else {
        char* mesg = "\nExiting foreground-only mode\n";
        gBackgroundStatus = 0;
        write(STDOUT_FILENO, mesg, 30);
    }
}

void handle_SIGCHLD(int signo, siginfo_t *info, void *ucontext){
    char *mesg = "\nbackground pid %d is done\n";
    char msg[50];
    pid_t chldPid = signo->si_pid;
    sprintf(msg, mesg, chldPid); 
    write(STDOUT_FILENO, msg, 50);
}



void newProcess(char *newargv[], int isInputFile, char *inputFile, int isOutputFile, char *outputFile, int forBack, int *curStatus){  
    
    
    int childStatus;
    pid_t spawnPid = fork();
    gSignalStatus = 0;
    
    switch(spawnPid){
        case -1:
            perror("fork()\n");
            exit(1);
            break;
        case 0:;
            
            if(isInputFile > 0){
                int source;
                source = open(inputFile, O_RDONLY);
                if (source == -1) { 
                    perror("source open()"); 
                    exit(1); 
                }
                int result = dup2(source, 0);
                if (result == -1) { 
                    perror("source dup2()"); 
                    exit(2); 
                }
                close(source);
            }
            if(isOutputFile > 0){
                int source;
                source = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (source == -1) { 
                    perror("source open()"); 
                    exit(1); 
                }
                int result = dup2(source, 1);
                if (result == -1) { 
                    perror("source dup2()"); 
                    exit(2); 
                }
                close(source);
            }
            execvp(newargv[0], newargv);
            perror("execvp");
            exit(2);
            break;
        default:
            if(forBack == 1){
                printf("Background pid is %d\n", spawnPid);
                spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
            }
            else {
                spawnPid = waitpid(spawnPid, &childStatus, 0);
                if(childStatus != 0){
                    *curStatus = 1;
                }
                else{
                    *curStatus = 0;
                }
            }
            break;
    }
    
}

/*
* First built-in command to change directories
* if no argument is given, directory is changed to HOME env directory
* otherwise, directory is changed to the argument given
*/
void cdCommand(char** arguments, int poscnt){
    char *curdir1 = getenv("HOME");
    if(poscnt > 0){
        if(arguments[0][0] == '/'){
            char* aPath = arguments[0];
            chdir(aPath);
        }
        else{
            char pPath[500]; 
            getcwd(pPath, 500);
            strcat(pPath, "/");
            strcat(pPath, arguments[0]);
            chdir(pPath);
        }

    }
    else {
        chdir(curdir1);
    }
}


int main(void){
    int alive = 1;
    int curStatus = 0;
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    struct sigaction SIGCHLD_action = {0};
    SIGCHLD_action.sa_handler = handle_SIGCHLD;
    sigfillset(&SIGCHLD_action.sa_mask);
    SIGCHLD_action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &SIGCHLD_action, NULL);

    while(alive){
        fflush(stdout);

        // Start Smallsh and promt user for input
        char input[2048];
        printf(": ");
        fgets(input, 2048, stdin);

        
        


        // Checks for input and output files as well as chars to store them
        int isInputFile = 0;
        int isOutputFile = 0;
        int isBackground = 0;

        char *inputFile;
        char *outputFile;
        char *arguments[512];

        // Processes the input by getting the first argument as the command
        // also removes the \n at the end of the input
        char *saveptr;
        char *errorFix;
        char *command;
        char *token;
        
        if(strlen(input) > 1){
            errorFix = strtok_r(input, "\n", &saveptr);
            token = strtok_r(errorFix, " ", &saveptr);
            command = calloc(strlen(token) + 1, sizeof(char));
            strcpy(command, token);
            token = strtok_r(NULL, " ", &saveptr);
        
        
            // Keeps track of the count in the arguments array
            int poscnt = 0;

            // While loop to analyze the arguments after the command
            // checks for < > and & and stores them in separate strings
            
            while(token != NULL){
                
                // Stores the argument relating to the input file and Output file
                if(isInputFile == 1){
                    inputFile = calloc(strlen(token) + 1, sizeof(char));
                    strcpy(inputFile, token);
                    isInputFile = 2;
                }
                else if(isOutputFile == 1){
                    outputFile = calloc(strlen(token) + 1, sizeof(char));
                    strcpy(outputFile, token);
                    isOutputFile = 2;
                }

                // Checks for <, >, and & and updates the loop to accept the next
                // argument as an input or output file
                if(strncmp(token, "<", 1) == 0){
                    isInputFile = 1;
                }
                else if(strncmp(token, ">", 1) == 0){
                    isOutputFile = 1;
                }
                else if(strncmp(token, "&", 1) == 0 && gBackgroundStatus == 0){
                    isBackground = 1;
                }

                // Main statement to copy the arguments that aren't input or output files
                // Has added functionallity to look for $$ and replace it with the pid
                if(isInputFile == 0 && isOutputFile == 0){
                    char* varEx = strstr(token, "$$");
                    if(varEx != NULL){
                        for(int i = 0; i < strlen(token); i++){
                            if(token[i] == '$' && token[i+1] == '$'){
                                token[i] = '%';
                                token[i+1] = 'd';
                            }
                        }
                    }
                    char proxy[512];
                    sprintf(proxy, token, getpid());
                    arguments[poscnt] = calloc(strlen(proxy) + 1, sizeof(char));
                    strcpy(arguments[poscnt], proxy);
                    poscnt++;
                }
              
                token = strtok_r(NULL, " ", &saveptr);
            }
          

            // After the the input from the command line is processed
            // the command is checked to dictate the next operation of the code
            if(strncmp(command, "exit", 4) == 0 && command != NULL){
                // Exit command that terminates the program
                alive = 0;
            }
            else if(strncmp(command, "#", 1) == 0 && command != NULL)
                /*comment indicator*/;
            else if(strncmp(command, "cd", 2) == 0 && command != NULL)
                cdCommand(arguments, poscnt);
            else if(strncmp(command, "status", 6) == 0 && command != NULL){
                // variable curStatus is changed as foregrond processes are ran
                // status call returns said status
                if(gSignalStatus > 0){
                    printf("terminated by signal %d\n", gSignalStatus);
                }
                else printf("exit value %d\n", curStatus);
            }
            else{
                
                // exec() call when there are arguments
                int posTracker = 1;
                char *newargv[poscnt + 2];
                newargv[0] = command;

                for(int i = 0; i < poscnt; i++){
                    newargv[posTracker] = arguments[i];
                    posTracker++;
                }

                newargv[posTracker] = NULL;
                newProcess(newargv, isInputFile, inputFile, isOutputFile, outputFile, isBackground, &curStatus);
            }

            //frees thre memory from the past arguments and commands
            for(int i=0; i< poscnt; i++){
                free(arguments[i]);
            }
            if(command != NULL)
                free(command);
            
            if(isInputFile > 0){
                free(inputFile);
            }
            if(isOutputFile > 0){
                free(outputFile);
            }
        }
    }

}