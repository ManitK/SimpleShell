#include "loader.h"
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// ADD ERROR HANDLING + MAGIC NUMBER
// TRY BONUS BY IMPLEMENTING WHILE LOOP
// ADD PREVIOUS COMMENTS

// Global Variables used throughout the code
Elf32_Ehdr *ehdr;
Elf32_Phdr *phdr;
int fd;
# define PAGE_SIZE 4096
void allocate_mem(Elf32_Phdr *segment, uintptr_t seg_fault_address) ;

int find_number_of_pages(int memory_size){
   return (memory_size/4096) + 1;
}

void allocate_memory_for_each_segment(){
   for (int i = 0; i < ehdr->e_phnum; i++) {
       Elf32_Phdr *phdr_entry = &phdr[i];
       uintptr_t page_start = phdr_entry->p_vaddr;
       mmap((void *)page_start,phdr_entry->p_memsz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
       }
   }

void sigsegv_handler(int signo, siginfo_t *info, void *context) {
   if (signo == SIGSEGV) {
       printf("Segmentation Fault captured.\n");
       uintptr_t seg_fault_address = (uintptr_t)info->si_addr;
       allocate_memory_for_each_segment();
       int i;
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
   uintptr_t page_size = 4096;
   //starting page address of segment where seg fault occured
   uintptr_t page_start = seg_fault_address & -page_size;
   //total count of pages needed to allocate
   size_t page_count = find_number_of_pages(segment->p_memsz);
   // Allocate memory for the segment
   void *allocated_memory = mmap((void *)page_start, page_count * page_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
   //Reading into the required segment
   off_t segment_offset = segment->p_offset;
   ssize_t segment_size = segment->p_filesz;
   lseek(fd,segment_offset,SEEK_SET);
   read(fd,allocated_memory,segment_size);
   // Calculating the new entrypoint within segment which caused segmentation fault
   void *new_entrypoint = (void *)(segment->p_vaddr + (seg_fault_address - page_start));
   // Cast the entry point to a function pointer
   int (*start)() = (int (*)())new_entrypoint;
}

void load_and_run_elf(char **argv) {
   fd = open(argv[1], O_RDONLY);
   if (fd == -1) {
       perror("open");
       exit(EXIT_FAILURE);
   }
   ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
   read(fd, ehdr, sizeof(Elf32_Ehdr));
   lseek(fd, ehdr->e_phoff, SEEK_SET);
 
   phdr = (Elf32_Phdr *)malloc(ehdr->e_phnum * sizeof(Elf32_Phdr));
   read(fd, phdr, ehdr->e_phnum * sizeof(Elf32_Phdr));
 
   struct sigaction catcher;
   catcher.sa_flags = SA_SIGINFO;
   catcher.sa_sigaction = sigsegv_handler;
   sigaction(SIGSEGV,&catcher,NULL);

   int (*start)() = (int (*)())ehdr->e_entry;
   int result = start();
   printf("User _start return value = %d\n", result);
}

int main(int argc, char **argv) {
   if (argc != 2) {
       printf("Usage: %s <ELF Executable>\n", argv[0]);
       exit(1);
   }

   if (access(argv[1], F_OK) == -1 || access(argv[1], R_OK) == -1) {
       perror("access");
       exit(1);
   }

   load_and_run_elf(argv);

   free(ehdr);
   free(phdr);
   close(fd);
 
   return 0;
}
