// #include "loader.h"
// #include <unistd.h>
// #include <signal.h>
// #include <setjmp.h>
// #include <sys/mman.h>
// #include <sys/wait.h>

// Elf32_Ehdr *ehdr;
// Elf32_Phdr *phdr;
// int fd;
// static jmp_buf jump_buffer;

// void page_fault_handler(int signum) {
//     // Handle the page fault here by mapping memory for the accessed address
//     void *page_address = (void *)((unsigned long)ehdr->e_entry & ~(4095));
//     int page_size = 4096; // Assuming a single page size (4KB) for simplicity

//     munmap(page_address, page_size);
//     if (mmap(page_address, page_size, PROT_READ | PROT_EXEC, MAP_PRIVATE,fd, 0) == MAP_FAILED) {

//         perror("mmap");
//         exit(EXIT_FAILURE);
//     }
//     // Re-register the signal handler, as it was unregistered due to the fault
//     signal(SIGSEGV, page_fault_handler);
//     longjmp(jump_buffer, 1); // Return to the point of the segmentation fault
// }

// void loader_cleanup() {
//     free(ehdr);
//     free(phdr);
// }

// void load_and_run_elf(char **argv) {
//     fd = open(argv[1], O_RDONLY);

//     int fd_error_check = 0;
//     if (fd == -1) {
//         fd_error_check++;
//     }

//     if (fd_error_check > 0) {
//         exit(EXIT_FAILURE);
//     }

//     off_t binary_size = lseek(fd, 0, SEEK_END);

//     char *binary_file = (char *)malloc(binary_size + 1);
//     ssize_t complete_file = read(fd, binary_file, binary_size);
//     binary_file[binary_size] = '\0';

//     lseek(fd, 0, SEEK_SET);

//     ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
//     ssize_t elf_header = read(fd, ehdr, sizeof(Elf32_Ehdr));

//     lseek(fd, ehdr->e_phoff, SEEK_SET);

//     phdr = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr));
//     read(fd, phdr, sizeof(Elf32_Phdr));

//     // Set up the signal handler for segmentation faults
//     signal(SIGSEGV, page_fault_handler);

//     if (setjmp(jump_buffer) == 0) {
//         void (*start)() = (void (*)())(ehdr->e_entry);
//         start(); // Attempt to run _start, this will generate a segmentation fault
//     } else {
//         // Page fault handler has executed, and memory is mapped
//         void (*start)() = (void (*)())(ehdr->e_entry);
//         start(); // Run _start again after memory is mapped
//     }

//     int status;
//     wait(&status); // Wait for the child process to exit

//     free(binary_file);
//     close(fd);
// }

// int main(int argc, char **argv) {
//     if (argc != 2) {
//         printf("Usage: %s <ELF Executable> \n", argv[0]);
//         exit(1);
//     }

//     int error_check_existance = 0;
//     int error_check_read_perm = 0;

//     if (access(argv[1], F_OK) == -1) {
//         error_check_existance++;
//     }
//     if (access(argv[1], R_OK) == -1) {
//         error_check_read_perm++;
//     }

//     if (error_check_existance > 0) {
//         printf("ELF File does not exist");
//         exit(1);
//     } else if (error_check_read_perm > 0) {
//         printf("Need permission for reading ELF\n");
//         exit(1);
//     }

//     load_and_run_elf(argv);

//     loader_cleanup();
//     return 0;
// }


// #include "loader.h"
 
// //added for using access() function needed for checks
// #include <unistd.h> 
 
// //Global Variables used throughout the code
// Elf32_Ehdr *ehdr;
// Elf32_Phdr *phdr;
// int fd;
 
// /*
//  * release memory and other cleanups
//  */
// void loader_cleanup() {
//   free(ehdr);
//   free(phdr);
// }
 
// void load_and_run_elf(char** argv) {
 
//   // Opened the ELF File given as input
//   fd = open(argv[1], O_RDONLY);
 
//   // Flag for checking if there is any error opening the input file
//   int fd_error_check = 0;
//   if (fd == -1) {
//         fd_error_check++;
//     }
 
//   if(fd_error_check>0){
//       exit(EXIT_FAILURE);
//   }
 
//   // Loaded the size of the binary file
//   off_t binary_size = lseek(fd, 0, SEEK_END);
 
//   // Memory allocation for storing binary file done by read into the file(+1 done to save null terminator)
//   char *binary_file = (char *)malloc(binary_size+1);
//   ssize_t complete_file = read(fd, binary_file, binary_size);
//   binary_file[binary_size] = '\0';
 
//   // Setting pointer to offset 0
//   lseek(fd, 0, SEEK_SET);
 
//   // Reading and allocating memory for ELF Header
//   ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
//   ssize_t elf_header = read(fd, ehdr, sizeof(Elf32_Ehdr)); 
 
//   // Setting pointer to the start of program header table
//   lseek(fd, ehdr->e_phoff, SEEK_SET);        
 
//   // Reading and allocating memory for Program Header Table
//   phdr = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr));
//   read(fd, phdr, sizeof(Elf32_Phdr));
 
//   // Making variable for memory allocation
//   void *virtual_mem;
 
//   // Making a flag variable for necessary checks
//   int flag_for_entrypoint = 0;
 
//   // While loop used for iterating through different Program Header entries which continues only when size of Program Header section is accurate
//   while (read(fd, phdr, sizeof(Elf32_Phdr)) == sizeof(Elf32_Phdr)) {
 
//         //Primary condition - when segment type is PT_LOAD and entrypoint addres is in 'p_vaddr'
//         if (phdr->p_type == PT_LOAD && ehdr->e_entry >= phdr->p_vaddr && ehdr->e_entry < (phdr->p_vaddr + phdr->p_memsz)) { 
//           printf("PT_LOAD segment detected with entrypoint address\n");
 
//           // Mapping memory for the segment
//           virtual_mem = mmap(NULL, phdr->p_memsz, PROT_READ | PROT_EXEC,MAP_PRIVATE,fd,phdr->p_offset);
//           flag_for_entrypoint++;
//         }
 
//         // Condition for when segment is PT_LOAD but entrypoint is not contained in 'p_vaddr'
//         else if( !(ehdr->e_entry >= phdr->p_vaddr && ehdr->e_entry < (phdr->p_vaddr + phdr->p_memsz)) && phdr->p_type == PT_LOAD){
//                     printf("PT_LOAD segment detected but entrypoint address is absent\n");
//         }
 
//         // Move file pointer to the next program header entry
//         lseek(fd,phdr->p_memsz,SEEK_CUR);
 
//         // Breaking out of while loop when entrypoint address is found
//         if(flag_for_entrypoint==1){
//           break;
//           }
//         }
 
//   // Defining a function pointer type
//   typedef int (*my_function)();
 
//   // Initializing locations which are needed for reaching the virtual address
//   void *address_offset1 = virtual_mem;
//   void *final_address = address_offset1 + (ehdr->e_entry - phdr->p_vaddr);
//   my_function start = (my_function)(final_address);
 
//   // Running the function which is pointing to the '_start' function
//   int result = start();
//   printf("User _start return value = %d\n", result);
 
//   // Cleaning up allocated memory and closing the file
//   munmap(virtual_mem, phdr->p_memsz);
//   free(binary_file);
//   close(fd);
// }
 
// int main(int argc, char** argv) 
// {
//   if(argc != 2) {
//     printf("Usage: %s <ELF Executable> \n",argv[0]);
//     exit(1);
//   }
 
//   // 1. carry out necessary checks on the input ELF file
//   int error_check_existance = 0;
//   int error_check_read_perm = 0;
 
//   if (access(argv[1], F_OK) == -1) {
//     error_check_existance++;
//   }
//   if (access(argv[1], R_OK) == -1) {
//     error_check_read_perm++;
//   }
 
//   if(error_check_existance>0){
//     printf("ELF File does not exist");
//     exit(1);
//   }
//   else if(error_check_read_perm>0){
//     printf("Need permission for reading ELF\n");
//     exit(1);
//   }
 
//   // 2. passing it to the loader for carrying out the loading/execution
//   load_and_run_elf(argv);
 
//   // 3. invoke the cleanup routine inside the loader  
//   loader_cleanup();
//   return 0;
// }

#include "loader.h"
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

// Global Variables
Elf32_Ehdr *ehdr;
int fd;

// Additional global variables to track memory mapping
void *mmap_addr = NULL;
size_t mmap_size = 0;
int page_faults = 0;
int page_allocations = 0;
size_t internal_fragmentation = 0;

// Signal handler for handling segmentation faults
void handle_segfault(int signum, siginfo_t *info, void *context) {
    // Calculate the page size and page-aligned address
    long page_size = sysconf(_SC_PAGESIZE);
    void *page_addr = (void *)((unsigned long)info->si_addr & ~(page_size - 1));

    // Map the page for the requested address
    if (mmap_addr != NULL) {
        munmap(mmap_addr, mmap_size);
    }
    mmap_addr = mmap(page_addr, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mmap_size = page_size;

    // Update internal fragmentation
    internal_fragmentation += page_size;

    page_faults++;
}

// Initialize the loader and install the signal handler
void init_loader(char **argv) {
    // Open the ELF file and read the ELF header
    fd = open(argv[1], O_RDONLY);

    // Install the signal handler for segmentation faults
    struct sigaction sig;
    sig.sa_sigaction = handle_segfault;
    sig.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sig, NULL);
    // Rest of the initialization (checking ELF file, etc.) remains the same
}

// Load and run the ELF program
void load_and_run_elf(char **argv) {
    // The loader's logic for loading segments lazily goes here
}

int main(int argc, char **argv) {
    // Initialize the loader
    init_loader(argv);

    // Rest of the main function remains the same
}
