#define _GNU_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
pid_t getpid(void);

int main(void){
    int alive = 1;
    while(alive){
        // Start Smallsh and pront user for input
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
        char *errorFix = strtok_r(input, "\n", &saveptr);
        char *token = strtok_r(errorFix, " ", &saveptr);
        char *command = calloc(strlen(token) + 1, sizeof(char));
        strcpy(command, token);
        
        // Keeps track of the count in the arguments array
        int poscnt = 0;

        // While loop to analyze the arguments after the command
        // checks for < > and & and stores them in separate strings
        token = strtok_r(NULL, " ", &saveptr);
        while(token != NULL){
            
            // Stores the argument relating to the input file and Output file
            if(isInputFile){
                inputFile = calloc(strlen(token) + 1, sizeof(char));
                strcpy(inputFile, token);
                isInputFile = 2;
            }
            else if(isOutputFile){
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

        if(strncmp(command, "exit", 4) == 0){
            alive = 0;
        }
        else if(strncmp(command, "#", 1));
        
        alive = 0;
        
    }

}