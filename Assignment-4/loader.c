#include "loader.h"
#include <unistd.h>
#include <signal.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// Global Variables used throughout the code
Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;
int page_faults = 0;
int times_memory_allocated = 0;
ssize_t overall_segment_size;


//release memory and other cleanups
void loader_cleanup() {
    //Flags for error handling to check ehdr and phdr before freeing them.
    int flag_for_ehdr_checking=0;
    int flag_for_phdr_checking=0;

    //updating flag values if EHDR AND PHDR are already NULL
    if (ehdr==NULL){flag_for_ehdr_checking++;}
    if (phdr==NULL){flag_for_phdr_checking++;}

  //Normal Running of code if flags not updated.
  if (flag_for_ehdr_checking==0 && flag_for_phdr_checking==0){
  free(ehdr);
  free (phdr);}

  //Error spotted if either of EHDR OR PHDR flag's value got updated.
  //This would indicate them being already NULl hence would leave nothing to free.

  else{
    printf("EHDR or PHDR is already NULL");
    exit(1);
  }

}

# define PAGE_SIZE 4096
void allocate_mem(Elf32_Phdr *segment, uintptr_t seg_fault_address) ;

int find_number_of_pages(int memory_size, int page_size){

  //creating a flag to spot error in this function.
  int flag_for_error_in_number_of_pages=0;

  //updating flag value if either of memory size or page size in negative which is not possible.
  //Negative values of these quantities would indicate an obvious error in our code.
  if (memory_size <= 0 || page_size <= 0) {
      flag_for_error_in_number_of_pages++;
    }

  //If flag is updated and not equal to the original value, we should insert an error and hence the code should be exited.
  if (flag_for_error_in_number_of_pages!=0){
      // Handling the error and returning an error code.
        perror("Invalid memory_size or page_size");

        exit(EXIT_FAILURE); // Indicates an error condition.
        return -1; 
  }

  //if flag value not updated and hence, no error spotted, we execute our code normally.
  else{
    if(memory_size%page_size == 0 ){
        return (memory_size/page_size);
    }
    else{
        return (memory_size/page_size) + 1;
    }
}
}

void sigsegv_handler(int signo, siginfo_t *info, void *context) {
    if (signo == SIGSEGV) {

        //Creating flags for spotting NULL values of context ot info.
        int flag_for_context=0;
        int flag_for_info=0;

        //Updating flag values if info or context is found to be NULL, which it should not be and hence an error is spotted.
        if (info==NULL){flag_for_info++;}
        if (context==NULL){flag_for_context++;}

        //Since the flag values are updated and not equal to the original values,
        //It indicates that NULL values were received hence an error must be inserted and code is exited. 
        if (flag_for_context!=0 || flag_for_info!=0){
          printf("SIGSEGV received NULL value for info or context");
          exit(EXIT_FAILURE);
        }

        //Executed if no error found
        printf("Segmentation Fault captured at address - %p\n",info->si_addr);
        page_faults++;
        uintptr_t seg_fault_address = (uintptr_t)info->si_addr;
        int i;

        //flags for spotting null values in ehdr or phdr
        int flag_for_ehdr_checking1=0;
        int flag_for_phdr_checking1=0;

        //Flag values are updated if EHDR or PHDR are found to be NULl
        if (ehdr==NULL){flag_for_ehdr_checking1++;}
        if (phdr==NULL){flag_for_phdr_checking1++;}

        //Since either the EHDR or PHDR value is found to be NULL which should not be the case, 
        //An error is spotted and hence the code is exited.
        if (flag_for_ehdr_checking1!=0 || flag_for_phdr_checking1!=0){
           printf("NULL value received for either the EDHR or the PHDR");
           exit(EXIT_FAILURE);}

        //Runs if no error is spotted and code has not been exited yet.
        for (i = 0; i < ehdr->e_phnum; i++) {
            Elf32_Phdr *segment = &phdr[i];
            uintptr_t segment_start = segment->p_vaddr;
            uintptr_t segment_end = segment_start + segment->p_memsz;

            //Finding the segment which caused the segmentation fault
            if (seg_fault_address >= segment_start && seg_fault_address < segment_end) {
                allocate_mem(segment, seg_fault_address);
                return;
            }
        }
    }
}


void allocate_mem(Elf32_Phdr *segment, uintptr_t seg_fault_address) {

    //Flag for spotting errors in allocating memory
    int flag_for_allocating_memory=0;

    int page_size = 4096;
    //starting page address of segment where seg fault occured
    uintptr_t page_start = seg_fault_address & -page_size;
    //total count of pages needed to allocate
    int page_count = find_number_of_pages(segment->p_memsz,page_size);
    
    // Allocate memory for the segment
    void *allocated_memory = mmap((void *)segment->p_vaddr, page_count * page_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    //If the allocation of memory if failed, we update the flag value to flag an error.
    if (allocated_memory == MAP_FAILED){
      flag_for_allocating_memory++;
    }

  //Since the flag value got updated, an error idicating that memory allocation wasn't successful is found.
  //This results in an error statement and exiting the code.
    if (flag_for_allocating_memory!=0){
      perror ("mmap error found in allocate_mem");
      exit(EXIT_FAILURE);
    }

    times_memory_allocated = times_memory_allocated + page_count;

    //Reading into the required segment
    off_t segment_offset = segment->p_offset;
    ssize_t segment_size = segment->p_memsz;
    lseek(fd,segment_offset,SEEK_SET);
    read(fd,allocated_memory,segment_size);

    //Global Variable
    overall_segment_size = overall_segment_size +  (page_count*page_size)-segment->p_memsz;
}


void load_and_run_elf(char **argv) {

    //Opened the ELF File given as input
    fd = open(argv[1], O_RDONLY);

    //Flag for checking if there is any error opening the input file
    int fd_error_check = 0;
    if (fd == -1) {
        fd_error_check++;}

    if(fd_error_check>0){
      perror("Error in opening the file !");
      exit(EXIT_FAILURE);  }


    // Reading and allocating memory for ELF Header
    ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
    read(fd, ehdr, sizeof(Elf32_Ehdr));

    //Setting pointer to the start of program header table
    lseek(fd, ehdr->e_phoff, SEEK_SET);

    //Reading and allocating memory for Program Header Table
    phdr = (Elf32_Phdr *)malloc(ehdr->e_phnum * sizeof(Elf32_Phdr));
    read(fd, phdr, ehdr->e_phnum * sizeof(Elf32_Phdr));

    //Flag for checking if sigaction works correctly.
    int flag_for_sigaction_checker=0;

    //Structure block for signal handling and catching a signal
    struct sigaction catcher;
    catcher.sa_flags = SA_SIGINFO;
    catcher.sa_sigaction = sigsegv_handler;

    //if running this statement returns a -1, we know an error must have occured. Hence we update the flag value.
    if (sigaction(SIGSEGV, &catcher, NULL) == -1) {
    flag_for_sigaction_checker++;}

    //An updated flag value indicates an error was spotted and hence the code must be exited here.
    if (flag_for_sigaction_checker!=0){
       perror("Error while registering for signal handler for SIGSEGV");
      exit(EXIT_FAILURE);
    }
    

    // Trying to run the function which is pointing to the '_start' function
    int (*start)() = (int (*)())ehdr->e_entry;
    int result = start();
    printf("User _start return value = %d\n", result);
}


int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <ELF Executable>\n", argv[0]);
        exit(1);
    }

  // 1. carry out necessary checks on the input ELF file
  int error_check_existance = 0;
  int error_check_read_perm = 0;

  if (access(argv[1], F_OK) == -1) {
    error_check_existance++;
  }
  if (access(argv[1], R_OK) == -1) {
    error_check_read_perm++;
  }

  if(error_check_existance>0){
    printf("ELF File does not exist");
    exit(1);
  }
  else if(error_check_read_perm>0){
    printf("Need permission for reading ELF\n");
    exit(1);
  }

    // 2. passing it to the loader for carrying out the loading/execution
    load_and_run_elf(argv);


    // 3. invoke the cleanup routine inside the loader and closing the file 
    loader_cleanup();
    close(fd);


    printf("\n");

    //Printing all other answers needed
    printf("Total Number of Page Faults: %d\n", page_faults);
    printf("Total Number of Page Allocations: %d\n", times_memory_allocated);
    printf("Total Amount of Internal Fragmentation:  %d\n", overall_segment_size);
    return 0;
}
