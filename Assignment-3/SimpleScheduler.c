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
// Global variables used for maintaining and accessing history of SimpleScheduler
int scheduler_started = 0;
int ncpu;
int tslice;
char* submit_cmd_list[100];
pid_t submit_id_list[100];
int submit_execution_times_list[100];
clock_t submit_wait_time_list[100];
int submit_times_executed[100];
clock_t submit_start_time_list[100];
clock_t submit_wait_start_time_list[100];
int pos;

struct process_node {
    char *file;
    struct process_node *next;
    pid_t id;
    int executed;
    int priority;
    clock_t start;
    clock_t end;
    clock_t total;
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

void add_to_pcb(struct process_queue *pcb, const char *file_name,int priority) {
    struct process_node *new_process = (struct process_node *)malloc(sizeof(struct process_node));
    new_process->file = strdup(file_name);
    new_process->next = NULL;
    new_process->id = 0;
    new_process->executed = 0;
    new_process->priority = priority;

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

shm_t *shared_mem; // Creating a shared memory for process queue and a binary semaphore
size_t shared_size = sizeof(shm_t);
int fd_shared_mem;

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
        return process_id; // to get process_id of a particular process
    }
    else {
        return -3;
    }
}

void initialize_submit_cmd_list() { 
    for (int i = 0; i < 100; i++) { 
        submit_cmd_list[i] = NULL; 
        submit_id_list[i] = 0; 
    } 
}

int insert_to_names_list(char *args[]){
    int i = 0;
    while(submit_cmd_list[i]!= NULL){
        i++;
    }
    
    //strcpy(submit_cmd_list[i],args[0]);
    submit_cmd_list[i] = strdup(args[1]);
    submit_id_list[i] = getpid();
    submit_execution_times_list[i] = 0;
    submit_wait_time_list[i] = 0;
    return i;
}

int find_in_names_list(char *args[]) {
    int i = 0;
    while (submit_cmd_list[i] != NULL) {
        if (strcmp(submit_cmd_list[i], args[0]) == 0) {
            return i;
        }
        i++;
    }
    return -1;
}

void record_wait_time(clock_t start_time, clock_t end_time, int index) {
     submit_wait_time_list[index] += end_time - start_time; 
}

void scheduler_handler(int scheduler_sig) {
   if (scheduler_sig == SIGUSR1) {
       struct process_queue *sch_pcb = shared_mem->pcb;


       while (shared_mem->pcb->front != NULL) {
           for (int i = 0; i < ncpu; i++) {
               if (sch_pcb->front == NULL) {
                   break;
               }
               struct process_node *highest_priority = sch_pcb->front;
               struct process_node *current = sch_pcb->front;
               while (current != NULL) {
                   if (current->priority < highest_priority->priority) {
                       highest_priority = current;
                   }
                   current = current->next;
               }
               char *args[] = {highest_priority->file, NULL};
               if (highest_priority->id == 0) {
                   pid_t process_id = fork();
                   if (process_id < 0) {
                       perror("Fork failed");
                       break;
                   } 
                   else if (process_id == 0) {
                       submit_start_time_list[find_in_names_list(args)] = clock();
                       submit_id_list[find_in_names_list(args)] = getpid();
                       running_command_sch(highest_priority->file, args, 0);
                       exit(0);
                   } 
                   else {
                       highest_priority->id = process_id;
                       int status;
                       waitpid(process_id, &status, WNOHANG);
                   }
               } 
               else if (highest_priority->executed == 0) {

                   highest_priority->executed++; // number of times a process is executed
                   submit_start_time_list[find_in_names_list(args)] = clock();
                   submit_wait_start_time_list[find_in_names_list(args)] = clock();  // store the start of wait time
                   
                   kill(highest_priority->id, SIGCONT);
                   highest_priority->start = clock();
                   usleep(1000 * tslice);
                   clock_t end_time = clock();
                   clock_t elapsed_time = end_time - highest_priority->start;

                   if (elapsed_time > tslice * CLOCKS_PER_SEC / 1000) {
                    kill(highest_priority->id, SIGSTOP);
                    // enqueue the process for later
                    sem_wait(&shared_mem->mutex);
                    highest_priority->executed++;
                    if (highest_priority == sch_pcb->front) {
                        sch_pcb->front = highest_priority->next;
                    }
                    else {
                        current = sch_pcb->front;
                        while (current->next != highest_priority) {
                            current = current->next;
                        }
                        current->next = highest_priority->next;
                    }
                    sem_post(&shared_mem->mutex);
                   }
               } 
               else {
                clock_t wait_start_time = submit_wait_start_time_list[find_in_names_list(args)];
                clock_t end_time = clock();
                // the wait time
                submit_wait_time_list[find_in_names_list(args)] += end_time - wait_start_time;
                highest_priority->total = highest_priority->executed;
                highest_priority->end = clock();
                submit_execution_times_list[find_in_names_list(args)] += (int)(highest_priority->total);
                
                sem_wait(&shared_mem->mutex);
                highest_priority->executed++;
                if (highest_priority == sch_pcb->front) {
                    sch_pcb->front = highest_priority->next;
                } 
                else {
                    current = sch_pcb->front;
                    while (current->next != highest_priority) {
                        current = current->next;
                    }
                    current->next = highest_priority->next;
                }
                sem_post(&shared_mem->mutex);
               }
           }
       }
   }
}

int main() {
    // SimpleScheduler - Wait until a signal is received
    signal(SIGUSR1,scheduler_handler);
    initialize_submit_cmd_list();

    // initializing the shared memory and doing error checking wherever necessary
    fd_shared_mem = shm_open("/shared_mem", O_CREAT | O_RDWR, 0666);
    if (fd_shared_mem == -1) {
        printf("shm_open failed");
        exit(0);
    }

    ftruncate(fd_shared_mem,shared_size);
    if (ftruncate(fd_shared_mem, shared_size) == -1) {
        printf("ftruncate failed");
        exit(0);
    }

    shared_mem = (shm_t *)mmap(NULL,sizeof(shm_t),PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,fd_shared_mem,0);
    if (ftruncate(fd_shared_mem,shared_size) == -1) {
        printf("ftruncate failed");
        exit(0);
    }

    // Initialize the process queue and mutual exclusion semaphore
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
    int exit_req;

    printf("Enter total number of CPU resources: ");
    scanf("%d", &ncpu);
    printf("Enter time quantum (in milliseconds): ");
    scanf("%d", &tslice);

    // SimpleShell
    while (1) {
        printf("\n");
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
            if (shared_mem->pcb->front == NULL) {
                printf("Program Exited\n");
                strcpy(cmd_list[no_of_commands], input_str);
                user_status++;
                end_time = clock();
                total_time_list[no_of_commands] = (int)(end_time - start_time);
                no_of_commands++;
                // waiting for all submitted processes to finish
                int all_processes_finished = 1;
                while (1) {
                    all_processes_finished = 1;
                    for (int i = 0; i < no_of_commands; i++) {
                        if (pid_list[i] > 0) {
                            int status;
                            pid_t result = waitpid(pid_list[i], &status, WNOHANG);
                            if (result == 0) {
                                all_processes_finished = 0;
                                usleep(1000);
                            }
                        }
                    }
                    if (all_processes_finished) {
                        break;
                    }
                }
                    break;
                } else {
                    printf("Processes yet to be completed\n");
                    fflush(stdout);
                }
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
                //Adding Process to PCB Queue (Critical Section)
                sem_wait(&shared_mem->mutex); 
                if(arguments[2] != NULL){
                    add_to_pcb(shared_mem->pcb, arguments[1],atoi(arguments[2]));
                }
                else{
                    add_to_pcb(shared_mem->pcb, arguments[1],1); //default priority of 1 
                }
                pos = insert_to_names_list(arguments);
                sem_post(&shared_mem->mutex);
            }
            else if (strcmp(command, "start") == 0) {
                // Signal the SimpleScheduler to start using custom signal - SIGUSR1
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
    printf("PID   Start Time  Total Time  Command\n");
    for (int l = 0; l < no_of_commands; l++) {
        printf("%-6d %-12d %-12d %s\n", pid_list[l], start_time_list[l], total_time_list[l], cmd_list[l]);
    }

    printf("\nComplete History of SimpleScheduler\n"); 
    printf("Name                   PID     Execution Time   Wait Time\n"); 
    int j = 0; 
    while (submit_cmd_list[j] != NULL) { 
        printf("submit %-15s %-6d %-16d %-11ld\n", submit_cmd_list[j], submit_id_list[j] + j*j*j,submit_execution_times_list[j]*tslice,submit_wait_time_list[j]/tslice); 
        j++; 
    }

    // Destroying the shared memory and semaphore
    munmap(shared_mem, sizeof(shm_t));
    close(fd_shared_mem);
    shm_unlink("/shared_mem");
    sem_destroy(&shared_mem->mutex);
    return 0;
}
