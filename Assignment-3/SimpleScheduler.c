#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

// Global variables used for maintaining and accessing history of SimpleShell
int pid_list[100];
int start_time_list[100];
clock_t start_time;
clock_t end_time;
int total_time_list[100];
int no_of_commands = 0;
char cmd_list[100][100];
volatile sig_atomic_t signum = 0;
int scheduler_started = 0;
int ncpu;
int tslice;

struct process_node {
    char *file;
    struct process_node *next;
    pid_t id;
    int executed;
    clock_t start;
    clock_t end;
};

struct process_queue {
    struct process_node *front;
    struct process_node *rear;
    int size;
};

struct process_queue *pcb_initialize() {
    struct process_queue *pcb = (struct process_queue *)malloc(sizeof(struct process_queue));
    if (pcb != NULL) {
        pcb->front = NULL;
        pcb->rear = NULL;
        pcb->size = 0;
    }
    return pcb;
}

void add_to_pcb(struct process_queue *pcb, const char *file_name) {
    struct process_node *new_process = (struct process_node *)malloc(sizeof(struct process_node));
    new_process->file = strdup(file_name);
    new_process->next = NULL;
    new_process->id = 0;
    new_process->executed = 0;

    if (pcb->front == NULL) {
        pcb->front = new_process;
        pcb->rear = new_process;
    } else {
        pcb->rear->next = new_process;
        pcb->rear = new_process;
    }
}

void sigint_handler() {
    printf("\nComplete History\n");
    printf("No.  PID    Start Time  Total Time  Command\n");
    for (int l = 0; l < no_of_commands; l++) {
        printf("%-4d %-6d %-12d %-12d %s\n", l + 1, pid_list[l], start_time_list[l], total_time_list[l], cmd_list[l]);
    }
    exit(0);
}

typedef struct shm_t {
    struct process_queue *pcb;
    sem_t mutex;
} shm_t;

int running_command(char *cmd, char *args[], int num_pipes) {
    if (num_pipes == 0) {
        pid_t process_id;
        int status;
        process_id = fork();
        pid_list[no_of_commands] = process_id;
        if (process_id < 0) {
            return -1;
        }
        else if (process_id == 0) {
            if (execvp(cmd, args) == -1) {
                return -2;
            }
        }
        else {
            waitpid(process_id, &status, 0);
        }
        return 0;
    }
    else {
        return -3;
    }
}

int running_command_sch(char *cmd, char *args[], int num_pipes) {
    if (num_pipes == 0) {
        pid_t process_id;
        int status;
        process_id = fork();
        pid_list[no_of_commands] = process_id;
        if (process_id < 0) {
            return -1;
        }
        else if (process_id == 0) {
            if (execvp(cmd, args) == -1) {
                return -2;
            }
        }
        else {
            //kill(process_id,SIGSTOP);
            waitpid(process_id, &status, 0);
        }
        return process_id;
    }
    else {
        return -3;
    }
}

// Creating a shared memory for process queue and a binary semaphore DO ERROR CHECKING
shm_t *shared_mem;
size_t shared_size = sizeof(shm_t);
int fd_shared_mem;

void scheduler_handler(int scheduler_sig) {
    if (scheduler_sig == SIGUSR1) {
        printf("Signal Received\n");
        struct process_queue *sch_pcb = shared_mem->pcb;

        while(shared_mem->pcb->front != NULL){
            for (int i = 0; i < ncpu; i++) {
            if (sch_pcb->front == NULL) {
                break;
            }

            struct process_node *current = sch_pcb->front;

            if (current->id == 0) {
                printf("FIRST TIME\n");
                char *args[] = {current->file, NULL};
                pid_t process_id = fork();

                if (process_id < 0) {
                    perror("Fork failed");
                    break;
                } else if (process_id == 0) {
                    running_command_sch(current->file, args, 0);
                    // Exit the child process after it's done
                    exit(0);
                } else {
                    current->id = process_id;
                    // Wait for the child process to finish and retrieve its status
                    int status;
                    waitpid(process_id, &status, 0);
                }
            } else if (current->executed == 0) {
                printf("REDOING\n");
                // Set the executed flag to indicate that the process is already running
                current->executed = 1;

                kill(current->id, SIGCONT);
                usleep(1000 * tslice);
                kill(current->id, SIGSTOP);
            } else {
                // The process has already been executed; dequeue it and free memory
                printf("Done - %s\n", current->file);

                sem_wait(&shared_mem->mutex);
                sch_pcb->front = current->next;
                free(current->file);
                free(current);
                sem_post(&shared_mem->mutex);
            }
        }
    }
    }
}

int main() {
    // SimpleScheduler - Wait until a signal is received
    signal(SIGUSR1,scheduler_handler);

    // DO ERROR CHECKING
    fd_shared_mem = shm_open("/shared_mem", O_CREAT | O_RDWR, 0666);
    ftruncate(fd_shared_mem,shared_size);
    shared_mem = (shm_t *)mmap(NULL,sizeof(shm_t),PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,fd_shared_mem,0);
    
    // Initialize the process queue and mutex semaphore
    shared_mem->pcb = pcb_initialize();
    sem_init(&shared_mem->mutex,1,1);

    signal(SIGINT, sigint_handler);
    char input_str[100];
    char *arguments[100];
    int user_status = 0;
    char *command;
    char *iterator;
    int pipes = 0;
    int state;

    printf("manitk@linux-desktop~$ Enter total number of CPU resources: ");
    scanf("%d", &ncpu);
    printf("manitk@linux-desktop~$ Enter time quantum (in milliseconds): ");
    scanf("%d", &tslice);

    // SimpleShell
    while (1) {
        printf("manitk@linux-desktop~$ ");
        fgets(input_str, sizeof(input_str), stdin);
        start_time = clock();

        if (strcmp(input_str, "\n") == 0) {
            continue;
        }
        else {
            start_time_list[no_of_commands] = start_time;
            input_str[strlen(input_str) - 1] = '\0';
            strcpy(cmd_list[no_of_commands], input_str);
        }

        if (strcmp(input_str, "exit") == 0) {
            start_time_list[no_of_commands] = start_time;
            printf("Program Exited\n");
            strcpy(cmd_list[no_of_commands], input_str);
            user_status++;
            end_time = clock();
            total_time_list[no_of_commands] = (int)(end_time - start_time);
            no_of_commands++;
            break;
        }
        else if (strcmp(input_str, "history") == 0) {
            start_time_list[no_of_commands] = start_time;
            strcpy(cmd_list[no_of_commands], input_str);
            int k = 0;
            printf("History of commands:\n");
            while (k < no_of_commands) {
                printf("%d  %s\n", k + 1, cmd_list[k]);
                k++;
            }
            end_time = clock();
            total_time_list[no_of_commands] = (int)(end_time - start_time);
            no_of_commands++;
        }
        else {
            pipes = 0;
            iterator = strtok(input_str, " ");
            command = iterator;
            int arg_length = 0;
            while (iterator != NULL) {
                arguments[arg_length] = iterator;
                if (strcmp(iterator, "|") == 0) {
                    pipes++;
                }
                iterator = strtok(NULL, " \n");
                arg_length++;
            }
            arguments[arg_length] = NULL;
            arg_length++;

            if (strcmp(command, "submit") == 0) {
                //Adding Process to PCB Queue
                sem_wait(&shared_mem->mutex);
                add_to_pcb(shared_mem->pcb, arguments[1]);
                sem_post(&shared_mem->mutex);
                //printf("%s \n",shared_mem->pcb->rear->file);
            }
            else if (strcmp(command, "start") == 0) {
                // Signal the SimpleScheduler to start
                kill(0,SIGUSR1);
            }
            else {
                state = running_command(command, arguments, pipes);
                if (state == -1) {
                    printf("Error occurred while forking\n");
                    break;
                }
                else if (state == -2) {
                    printf("Error occurred while executing the command\n");
                    break;
                }
                else if (state == -3) {
                    printf("External Error\n");
                    break;
                }
            }

            end_time = clock();
            total_time_list[no_of_commands] = (int)(end_time - start_time);
            no_of_commands++;
        }
    }

    printf("\nComplete History\n");
    printf("No.  PID    Start Time  Total Time  Command\n");
    for (int l = 0; l < no_of_commands; l++) {
        printf("%-4d %-6d %-12d %-12d %s\n", l + 1, pid_list[l], start_time_list[l], total_time_list[l], cmd_list[l]);
    }

    munmap(shared_mem, sizeof(shm_t));
    close(fd_shared_mem);
    shm_unlink("/shared_mem");
    sem_destroy(&shared_mem->mutex);
    return 0;
}
