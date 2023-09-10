#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

// global variables used for maintaing and accessing history of SimpleShell
int pid_list[100];
int start_time_list[100];
clock_t start_time;
clock_t end_time;
int total_time_list[100];
int no_of_commands = 0;
char cmd_list[100][100];
volatile sig_atomic_t signum = 0;

// Signal handler
void sigint_handler() {
    printf("\nComplete History\n");
    printf("No.  PID    Start Time  Total Time  Command\n");
    for (int l = 0; l < no_of_commands; l++) {
        printf("%-4d %-6d %-12d %-12d %s\n", l + 1, pid_list[l], start_time_list[l], total_time_list[l], cmd_list[l]);
    }
    exit(0);
}

int running_command(char *cmd,char *args[],int num_pipes){
    if(num_pipes == 0){ // when no piping is to be done
        pid_t process_id;
        int status;
        process_id = fork();
        pid_list[no_of_commands] = process_id;
        if (process_id < 0) {
            return -1;
        }
        else if (process_id == 0) {
            if (execvp(cmd, args) == -1) { // Child process
                return -2;
            }
        }
        else {
            waitpid(process_id, &status, 0); // Parent process
        }
    return 0;
    }
    else{
        return -4;
    }
}

int running_command_with_pipes(char *args[], int num_pipes, char *input_str) {
    char *iterator;
    iterator = strtok(input_str, "|"); // Split the input string with respect to pipe(|)
    int arg_length = 0;

    while (iterator != NULL) {      // Store the splitted string in args array
        args[arg_length] = iterator;
        arg_length++;
        iterator = strtok(NULL, "|"); // Go to the next splitted string
    }
    args[arg_length] = NULL;

    int num_commands = num_pipes + 1; // Create a pipeline of num_pipes + 1 processes
    int pipe_fds[num_pipes][2];
    pid_t child_pids[num_commands];   // Store the pids of the child processes
    pid_t process_id;
    process_id = fork();
    pid_list[no_of_commands] = process_id;

    for (int i = 0; i < num_commands; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            return -3;
        }
    }

    char storage[1000];
    FILE *pipe_fp;
    pipe_fp = popen(input_str, "r");
    if (pipe_fp == NULL) {
        return -5;
    }
    while (fgets(storage, sizeof(storage), pipe_fp) != NULL) {
        printf("%s",storage);
    }
    pclose(pipe_fp);
    return 0;
}

int main(){
    signal(SIGINT, sigint_handler);
    char input_str[100];
    char input_str_copy[100];
    char *arguments[100];
    char *arguments_with_pipes[100];
    int user_status = 0;
    char *command; 
    char *iterator;
    int pipes = 0;
    int state;

    while(1){
        printf("\n");
        printf("manitk@linux-desktop~$ ");
        fgets(input_str, sizeof(input_str), stdin); // Taking input command from user
        strcpy(input_str_copy, input_str);
        start_time = clock();

        if( strcmp(input_str, "\n") == 0){ // when only enter is pressed
            continue;
        }
        else{ // when some command is entered with enter, so newline character has to be removed
            start_time_list[no_of_commands] = start_time;
            input_str[strlen(input_str) - 1] = '\0';
            strcpy(cmd_list[no_of_commands], input_str); // Use strcpy to copy input_str into cmd_list
        }

        if ( strcmp(input_str, "exit") == 0) { // when user wants to exit the program
            start_time_list[no_of_commands] = start_time;
            printf("Program Exited\n");
            strcpy(cmd_list[no_of_commands], input_str);
            user_status++;
            end_time = clock();
            total_time_list[no_of_commands] = (int)(end_time - start_time);
            no_of_commands++;
            break;
        }
        else if(strcmp(input_str, "history") == 0){ // when user asks for history of shell
            start_time_list[no_of_commands] = start_time;
            strcpy(cmd_list[no_of_commands], input_str);
            int k = 0;
            printf("history of commands:\n"); // printing history of commands
            while(k<no_of_commands){
                printf("%d  %s\n",k+1,cmd_list[k]);
                k++;
            }
            end_time = clock();
            total_time_list[no_of_commands] = (int)(end_time - start_time);
            no_of_commands++;
        }
        else{ // when command entered is to be executed
            //parsing the command with the respect to spaces(" ")
            pipes = 0;
            iterator = strtok(input_str," ");
            command = iterator; // first word of input string

            int arg_length = 0;
            while (iterator != NULL) {
                arguments[arg_length] = iterator;
                if(strcmp(iterator,"|")==0){
                    pipes++;
                }
                iterator = strtok(NULL," \n"); // Going to the next word in the input string
                arg_length++;
            }

            // BONUS PART - when user wants to run bash or sh file
            if((strcmp(command,"bash") == 0) || (strcmp(command,"sh") == 0)){ 
                char storage[1000];
                FILE *file;
                file = popen(arguments[1], "r");
                if (file == NULL) {
                    printf("Error occured while opening file.\n");
                    break;
                }
                while (fgets(storage, sizeof(storage), file) != NULL) {
                    printf("%s",storage);
                }
                pclose(file);
            }

            if(pipes == 0){ // when no piping is to be done, this loop is triggered
                arguments[arg_length] = NULL;
                arg_length++;
                state = running_command(command,arguments,pipes);
            }
            else{
                // when piping is to be done, this loop is triggered
                state = running_command_with_pipes(arguments_with_pipes, pipes, input_str_copy);
            }

            // Error handling
            if(state == -1){ 
                printf("Error occured while forking\n");
                break;
            }
            else if(state == -2){
                printf("Error occured while executing the command\n");
                break;
            }
            else if(state == -3){
                printf("Error occured while piping\n");
                break;
            }
            else if(state == -4){
                printf("Error occured while executing the command in piping process\n");
                break;
            }
            else if(state == -5){
                printf("Error occured while showing output after pipelining\n");
                break;
            }
            end_time = clock();
            total_time_list[no_of_commands] = (int)(end_time - start_time);
            no_of_commands++;
        }
    }

    // printing all the history stored and collected while running SimpleShell
    printf("\n");
    printf("Complete History\n");
    printf("No.  PID    Start Time  Total Time  Command\n");
    for (int l = 0; l < no_of_commands; l++) {
        printf("%-4d %-6d %-12d %-12d %s\n", l + 1, pid_list[l], start_time_list[l], total_time_list[l], cmd_list[l]);
    }
    return 0;
}