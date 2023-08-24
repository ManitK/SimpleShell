#include "loader.h"

//added for using access() function needed for checks
#include <unistd.h> 

//Global Variables used throughout the code
Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;

/*
 * release memory and other cleanups
 */
void loader_cleanup() {
  free(ehdr);
  free(phdr);
}

void load_and_run_elf(char** argv) {

  // Opened the ELF File given as input
  fd = open(argv[1], O_RDONLY);

  // Flag for checking if there is any error opening the input file
  int fd_error_check = 0;
  if (fd == -1) {
        fd_error_check++;
    }

  if(fd_error_check>0){
      exit(EXIT_FAILURE);
  }

  // Loaded the size of the binary file
  off_t binary_size = lseek(fd, 0, SEEK_END);
  
  // Memory allocation for storing binary file done by read into the file(+1 done to save null terminator)
  char *binary_file = (char *)malloc(binary_size+1);
  ssize_t complete_file = read(fd, binary_file, binary_size);
  binary_file[binary_size] = '\0';

  // Setting pointer to offset 0
  lseek(fd, 0, SEEK_SET);

  // Reading and allocating memory for ELF Header
  ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
  ssize_t elf_header = read(fd, ehdr, sizeof(Elf32_Ehdr)); 

  // Setting pointer to the start of program header table
  lseek(fd, ehdr->e_phoff, SEEK_SET);        

  // Reading and allocating memory for Program Header Table
  phdr = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr));
  read(fd, phdr, sizeof(Elf32_Phdr));

  // Making variable for memory allocation
  void *virtual_mem;

  // Making a flag variable for necessary checks
  int flag_for_entrypoint = 0;

  // While loop used for iterating through different Program Header entries which continues only when size of Program Header section is accurate
  while (read(fd, phdr, sizeof(Elf32_Phdr)) == sizeof(Elf32_Phdr)) {

        //Primary condition - when segment type is PT_LOAD and entrypoint addres is in 'p_vaddr'
        if (phdr->p_type == PT_LOAD && ehdr->e_entry >= phdr->p_vaddr && ehdr->e_entry < (phdr->p_vaddr + phdr->p_memsz)) { 
          printf("PT_LOAD segment detected with entrypoint address\n");

          // Mapping memory for the segment
          virtual_mem = mmap(NULL, phdr->p_memsz, PROT_READ | PROT_EXEC,MAP_PRIVATE,fd,phdr->p_offset);
          flag_for_entrypoint++;
        }

        // Condition for when segment is PT_LOAD but entrypoint is not contained in 'p_vaddr'
        else if( !(ehdr->e_entry >= phdr->p_vaddr && ehdr->e_entry < (phdr->p_vaddr + phdr->p_memsz)) && phdr->p_type == PT_LOAD){
                    printf("PT_LOAD segment detected but entrypoint address is absent\n");
        }

        // Move file pointer to the next program header entry
        lseek(fd,phdr->p_memsz,SEEK_CUR);

        // Breaking out of while loop when entrypoint address is found
        if(flag_for_entrypoint==1){
          break;
          }
        }

  // Defining a function pointer type
  typedef int (*my_function)();

  // Initializing locations which are needed for reaching the virtual address
  void *address_offset1 = virtual_mem;
  void *final_address = address_offset1 + (ehdr->e_entry - phdr->p_vaddr);
  my_function start = (my_function)(final_address);

  // Running the function which is pointing to the '_start' function
  int result = start();
  printf("User _start return value = %d\n", result);

  // Cleaning up allocated memory and closing the file
  munmap(virtual_mem, phdr->p_memsz);
  free(binary_file);
  close(fd);
}

int main(int argc, char** argv) 
{
  if(argc != 2) {
    printf("Usage: %s <ELF Executable> \n",argv[0]);
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

  // 3. invoke the cleanup routine inside the loader  
  loader_cleanup();
  return 0;
}