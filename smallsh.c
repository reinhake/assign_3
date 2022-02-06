#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>


void newProcess(char *newargv[], int isInputFile, char *inputFile, int isOutputFile, char *outputFile){  
    int childStatus;
    pid_t spawnPid = fork();

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
                int result = dup2(source, 0);
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
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            break;
    }
        
    
}


int main(void){
    int alive = 1;
    
    while(alive){
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
                else if(strncmp(token, "&", 1) == 0){
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
          



            if(strncmp(command, "exit", 4) == 0 && command != NULL){
                alive = 0;
            }
            else if(strncmp(command, "#", 1) == 0 && command != NULL);
            else if(strncmp(command, "cd", 2) == 0 && command != NULL){
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
            else if(strncmp(command, "status", 6) == 0 && command != NULL){
                
            }
            else{
                if(poscnt > 0){
                    char *newargv[] = {command, *arguments, NULL };
                    newProcess(newargv, isInputFile, inputFile, isOutputFile, outputFile);
                }
                else{ 
                    char *newargv[] = {command, NULL };
                    newProcess(newargv, isInputFile, inputFile, isOutputFile, outputFile);
                }
            }

            
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