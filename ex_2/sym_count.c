#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>

#define PIPE_READ_BUF_SIZE 256 

//need these to be global for the SIGPIPE and error handlers
char sym;
int counter = 0;	
int fd = -1;
char *file_arr = NULL;
int file_size = 0;
int pipe_fd = -1;
char *file_path;


void handle_error(const char *msg){
	perror(msg);
	if (file_arr != NULL)
        	munmap(file_arr, file_size);
        if (fd != -1) 
		close(fd);
	if (pipe_fd != -1)
		close(pipe_fd);
        exit(EXIT_FAILURE);
}

void map_file_to_mem(char* file_path){
	fd = open(file_path, O_RDWR, 0600);
	if (fd == -1)
		handle_error("Opening file failed");
 	struct stat sb;
	if (lstat(file_path, &sb) == -1)
		handle_error("lstat failed");
	file_size = sb.st_size;
	if(lseek(fd, file_size-1, SEEK_SET) == -1)
		handle_error("Error calling lseek() to strech the file");
	if(write( fd, "", 1 ) != 1)
		handle_error("Error writing the last byte of the file");
	file_arr = (char*)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (file_arr == MAP_FAILED)
		handle_error("sym_count failed mapping to memory");	
}

void my_sigpipe_handler(int signum){
	printf("SIGPIPE for process %d. Symbol %c. Counter %d. Leaving.\n", getpid(), sym, counter);
	if (file_arr != NULL)
		munmap(file_arr, file_size);
	if (fd != -1) 
		close(fd);
	if (pipe_fd != -1)
		close(pipe_fd);
	exit(EXIT_FAILURE);
}

void register_sigpipe(){
  	// Structure to pass to the registration syscall
	struct sigaction my_sigpipe_action;
	memset(&my_sigpipe_action, 0, sizeof(my_sigpipe_action));
	// Assign pointer to our handler function
  	my_sigpipe_action.sa_handler = my_sigpipe_handler;
  	// Register the handler
  	if(sigaction(SIGPIPE, &my_sigpipe_action, NULL) != 0){
		handle_error("SIGPIPE registration failed");
  	}
}

void my_sigterm_handler(int signum){
	if (file_arr != NULL)
        	munmap(file_arr, file_size);
	if (fd != -1) 
		close(fd);
	if (pipe_fd != -1)
		close(pipe_fd);
	exit(EXIT_FAILURE);
}

void register_sigterm(){
  	// Structure to pass to the registration syscall
	struct sigaction my_sigterm_action;
	memset(&my_sigterm_action, 0, sizeof(my_sigterm_action));
	// Assign pointer to our handler function
  	my_sigterm_action.sa_handler = my_sigterm_handler;
  	// Register the handler
  	if(sigaction(SIGTERM, &my_sigterm_action, NULL)!=0){
		handle_error("SIGTERM registration failed");
  	}
}

int main(int argc, char** argv){
	//assert number of arguments and type of second argument
	assert(argc == 4);
        assert(strlen(argv[2]) == 1);
        //register the SIGPIPE and SIGTERM handlers
	register_sigpipe();
	register_sigterm();
	//load args and map file into memory
	sym = argv[2][0];
	pipe_fd = atoi(argv[3]);
	map_file_to_mem(argv[1]);//now file_arr and file_size have the correct values 
	//go over the data and update the instance counter every time sym is encountered			
	int i;
	for (i = 0; i < file_size; i++){
		if (file_arr[i] == sym)
			counter++;	
	}
	//Report to the manager
	char read_buffer[PIPE_READ_BUF_SIZE];
	sprintf(read_buffer, "Process %d finishes. Symbol %c. Instances %d.\n", getpid(), sym, counter);
        if (write(pipe_fd, read_buffer, strlen(read_buffer)) == -1)
		handle_error("writing to pipe failed");
	close(pipe_fd);
	if (munmap(file_arr, file_size) == -1)
        	handle_error("Failed un-mapping file");
	close(fd);
	return EXIT_SUCCESS;
}
