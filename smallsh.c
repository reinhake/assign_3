#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

volatile sig_atomic_t gStatus = 0;
volatile sig_atomic_t gSignalStatus = 0;
volatile sig_atomic_t gBackgroundStatus = 0;
volatile sig_atomic_t gIsChild = 0;
volatile sig_atomic_t gPidCount = 0;

// function to print the status with two different options
// the first is if the last foreground process was terminated
// the second is if it was exited normally
void printStatus(){
    if(gSignalStatus > 0 && gIsChild == 1){
        gIsChild = 0;
        char *mesg = "terminated by signal %d\n";
        char msg[25];
        snprintf(msg, 25, mesg, gSignalStatus); 
        write(STDOUT_FILENO, msg, 25);
    }
    else if (gSignalStatus == 0 && gIsChild == 1){ 
        gIsChild = 0;
        char *mesg = "exit value %d\n";
        char msg[15];
        snprintf(msg, 15, mesg, gStatus); 
        write(STDOUT_FILENO, msg, 15);
    }
}

// handler function for SIGINT that calls printStatus and tells
// it that the last function was terminated by it
void handle_SIGINT(int signo){
    gSignalStatus = signo;
    printStatus();
}

// Handler function SIGTSTP that changes whether or not background
// processes are allowed
// does so by changing a global variable
void handle_SIGTSTP(int signo){
    if (gBackgroundStatus == 0){
        gBackgroundStatus = 1;
        char *mesg = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, mesg, 49);
    }
    else {
        char* mesg = "Exiting foreground-only mode\n";
        gBackgroundStatus = 0;
        write(STDOUT_FILENO, mesg, 29);
    }
}

// function for any processes that are not built in
// its parameters are the list of arguments, InputFiles (if any), OutputFiles (if any),
// the checker for background processes, and finally the array of background processes
void newProcess(char *newargv[], int isInputFile, char *inputFile, int isOutputFile, char *outputFile, int forBack, int* pidArray){  
    // Create a forked child process
    int childStatus;
    pid_t spawnPid = fork();
    // update global variables so calls and signals function properly
    gSignalStatus = 0;
    gIsChild = 1;
    
    // Enter Child process
    switch(spawnPid){
        case -1:
            //catch error in the forking process
            perror("fork()\n");
            exit(1);
            break;
        case 0:;
            // Check and open the input files
            if(isInputFile > 0){
                int source;
                // open scource file
                source = open(inputFile, O_RDONLY);
                if (source == -1) { 
                    printf("cannot open %s for input\n", inputFile); 
                    exit(1); 
                }
                // duplicate source file to STDIN
                int result = dup2(source, 0);
                if (result == -1) { 
                    perror("source dup2()"); 
                    exit(2); 
                }
                close(source);
            }
            // Check and open the output files
            if(isOutputFile > 0){
                int source;
                // open scource file
                source = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (source == -1) { 
                    perror("source open()");
                    exit(1); 
                }
                // duplicate source file to STDOUT
                int result = dup2(source, 1);
                if (result == -1) { 
                    perror("source dup2()"); 
                    exit(2); 
                }
                close(source);
            }
            //call exec() and run the command while catching any errors
            execvp(newargv[0], newargv);
            perror(newargv[0]);
            exit(2);
            break;
        default:
            //verify if this process is to be foreground or background
            if(forBack == 1){// is background
                // print pid of background process and store it in array
                // so that it can be checked until it finishes or is terminated
                printf("background pid is %d\n", spawnPid);
                pidArray[gPidCount] = spawnPid;
                gPidCount++;
                spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
            }
            else {// is foreground
                // wait for process to finish running then update the global status
                spawnPid = waitpid(spawnPid, &childStatus, 0);
                if(childStatus != 0){
                    gStatus = 1;
                    gIsChild = 0;
                }
                else if (childStatus == 0){
                    gStatus = 0;
                    gIsChild = 0;
                }
            }
            break;
    }
    
}


// First built-in command to change directories
// if no argument is given, directory is changed to HOME env directory
// otherwise, directory is changed to the argument given

void cdCommand(char** arguments, int poscnt){
    char *curdir1 = getenv("HOME");
    //check if there are any arguments for cd
    if(poscnt > 0){
        if(arguments[0][0] == '/'){// process absolute positions
            char* aPath = arguments[0];
            chdir(aPath);
        }
        else{// process relative
            char pPath[500]; 
            getcwd(pPath, 500);
            strcat(pPath, "/");
            strcat(pPath, arguments[0]);
            chdir(pPath);
        }

    }
    else {// no arguments so cd redirects to HOME enviroment designated directory
        chdir(curdir1);
    }
}


int main(void){
    // start and exit variable for the while loop
    int alive = 1;

    // stores background pid's
    int pidArray[20];

    // handler for SIGINT that will call my handler function
    // whenever CNTR+C is called
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = handle_SIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // handler for SIGTSTP that will call my handler function
    // whenever CNTR+C is called
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // main loop that keeps the program running
    // starts by checking background processes for any that have been finished
    // then promts the user and processes their inputs
    // any built-in function are run in the main process
    // anything else is funned into a child process and uses exec to call
    while(alive){
        // loops through any background processes that have been called
        // to search for any that have been terminated or finished running
        // waitpid is used to check if the process was been terminated by a signal or not
        int finishedChild = 0;
        for(int i = 0; i < gPidCount; i++){
            int Child;
            int checkChild = waitpid(pidArray[i], &Child, WNOHANG);
            if(checkChild != 0){
                printf("background pid %d has exited", pidArray[i]);
                if(WIFSIGNALED(Child)){
                    int sig = WTERMSIG(Child);
                    printf(": terminated by %d", sig);
                }
                else printf(": exit status 0");
                finishedChild++;
                printf("\n");
            }
        }
        gPidCount = gPidCount - finishedChild;

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
        
        // Makes sure the command line is not empty before turning arguments into
        // tokens and then storing them
        if(strlen(input) > 1){
            // remove the new line operator
            errorFix = strtok_r(input, "\n", &saveptr);
            // Turn the first argument into a token and store it in command
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
                    // adds the pid to %d if there was a $$
                    if(strncmp(token, "&", 1) != 0){
                        char proxy[512];
                        sprintf(proxy, token, getpid());
                        arguments[poscnt] = calloc(strlen(proxy) + 1, sizeof(char));
                        strcpy(arguments[poscnt], proxy);
                        poscnt++;
                    }
                }
                // get the next token and continue looping
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
                gIsChild = 1;
                printStatus();
            }
            else{

                // any commands that don't fit into the build in command (cd, status, etc.)
                // will be turned into an argument array so they can be called on by exec()
                int posTracker = 1;
                char *newargv[poscnt + 2];
                newargv[0] = command;
                for(int i = 0; i < poscnt; i++){
                    newargv[posTracker] = arguments[i];
                    posTracker++;
                }
                newargv[posTracker] = NULL;
                newProcess(newargv, isInputFile, inputFile, isOutputFile, outputFile, isBackground, pidArray);
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