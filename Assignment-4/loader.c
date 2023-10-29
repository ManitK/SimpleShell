#include "loader.h"
 #include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

// Global Variables used throughout the code
Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;
void allocate_mem(int size_of_allocation);

/*
 * release memory and other cleanups
 */
void loader_cleanup() {
  free(ehdr);
  free(phdr);
}
void sigint_handler(){
  printf("SegFault captured. ");
  allocate_mem((int)4096);
  exit(EXIT_SUCCESS);

}

void allocate_mem(int size_of_allocation){
  signal(SIGSEGV,sigint_handler);
  void *virtual_mem;
  size_t size_page=size_of_allocation;
  virtual_mem=mmap(NULL, size_page, PROT_READ | PROT_EXEC, MAP_PRIVATE,fd,phdr->p_offset );
  printf("Alloted Memory of 4kb successfully! ");

    // Defining a function pointer type
  typedef int (*my_function)();

  // Initializing locations which are needed for reaching the virtual address
  void *address_offset1 = virtual_mem;
  void *final_address = address_offset1 + (ehdr->e_entry - phdr->p_vaddr);
  my_function start = (my_function)(final_address);

  // Running the function which is pointing to the '_start' function
  int result = start();
  printf("User _start return value = %d\n", result);

}

void load_and_run_elf(char **argv) {

  // Opened the ELF File given as input
  fd = open(argv[1], O_RDONLY);

  // Flag for checking if there is any error opening the input file
  int fd_error_check = 0;
  if (fd == -1) {
    fd_error_check++;
  }

  if (fd_error_check > 0) {
    exit(EXIT_FAILURE);
  }

  // Reading and allocating memory for ELF Header
  ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
  ssize_t elf_header = read(fd, ehdr, sizeof(Elf32_Ehdr));

  // Setting pointer to the start of program header table
  lseek(fd, ehdr->e_phoff, SEEK_SET);

  // Reading and allocating memory for Program Header Table
  phdr = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr));
  read(fd, phdr, sizeof(Elf32_Phdr));

  // Making a flag variable for necessary checks
  int flag_for_entrypoint = 0;

  // While loop used for iterating through different Program Header entries
  while (read(fd, phdr, sizeof(Elf32_Phdr)) == sizeof(Elf32_Phdr)) {
    // Primary condition - when segment type is PT_LOAD and entrypoint address is in 'p_vaddr'
    if (phdr->p_type == PT_LOAD && ehdr->e_entry >= phdr->p_vaddr && ehdr->e_entry < (phdr->p_vaddr + phdr->p_memsz)) {
      printf("PT_LOAD segment detected with entrypoint address\n");

      // No need to map memory, directly attempt to run _start method
      void *entrypoint = (void *)(ehdr->e_entry);
      int (*start)() = entrypoint;
      int result = start();
      //printf("User _start return value = %d\n");
      flag_for_entrypoint++;
      return;
    }

    // Move file pointer to the next program header entry
    lseek(fd, phdr->p_memsz, SEEK_CUR);

    // Breaking out of while loop when entrypoint address is found
    if (flag_for_entrypoint == 1) {
      break;
    }
  }

  // If no suitable segment with the entrypoint is found, exit with an error message.
  printf("No suitable PT_LOAD segment with entrypoint address found.\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: %s <ELF Executable>\n", argv[0]);
    exit(1);
  }
signal(SIGSEGV,sigint_handler);
  // 1. carry out necessary checks on the input ELF file
  int error_check_existance = 0;
  int error_check_read_perm = 0;

  if (access(argv[1], F_OK) == -1) {
    error_check_existance++;
  }
  if (access(argv[1], R_OK) == -1) {
    error_check_read_perm++;
  }

  if (error_check_existance > 0) {
    printf("ELF File does not exist");
    exit(1);
  } else if (error_check_read_perm > 0) {
    printf("Need permission for reading ELF\n");
    exit(1);
  }

  // 2. passing it to the loader for carrying out the loading/execution
  load_and_run_elf(argv);

  

  // 3. invoke the cleanup routine inside the loader
  loader_cleanup();
  return 0;
}
