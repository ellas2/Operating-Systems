#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <errno.h>

#define WRITE_PIPE_FD_BUF_SIZE 20

//need these to be global for the SIGPIPE and error handlers
int* managed_pids;
int* pipe_read_fds;
int actual_num_processes = 0;

void handle_error(const char *msg){
	perror(msg);
	free(managed_pids);
	int j;
	for(j = 0; j < actual_num_processes; j++){
		if (pipe_read_fds[j] != -1)
			close(pipe_read_fds[j]);
	}
	free(pipe_read_fds);
        exit(EXIT_FAILURE);
}

void initialize_processes(char* file_path, char* pattern){
	int index;	
	int curr_child = -1;
	for (index=0; index<actual_num_processes; index++){
		int pipefds[] = {-1,-1};
		pipe(pipefds);
		if((curr_child = fork()) == 0){
			//child code
			if (close(pipefds[0]) == -1)
				handle_error("close failed");
			char str_write_pipe_fd[WRITE_PIPE_FD_BUF_SIZE];
			sprintf(str_write_pipe_fd,"%d",pipefds[1]);
			char pattern_ind[] = {pattern[index], '\0'};
			char* exec_args[] = {"./sym_count", file_path, pattern_ind, str_write_pipe_fd, NULL};
			if (execvp(exec_args[0], exec_args) == -1)
				handle_error("execvp failed");		
		}
		else{
			//fork failed
			if (curr_child == -1)
				handle_error("fork failed");
			//parent code
			close(pipefds[1]);	
			managed_pids[index] = curr_child;
			pipe_read_fds[index] = pipefds[0];
		}	
        }
}


void terminate_all_children(){
	int i;
	for (i = 0; i < actual_num_processes; i++){
		kill(managed_pids[i], SIGTERM);
	}
}

void exclude_process_from_list(int index){
	managed_pids[index] = managed_pids[actual_num_processes];
	pipe_read_fds[index] = pipe_read_fds[actual_num_processes];
	int* new_managed_pids = realloc(managed_pids, actual_num_processes*sizeof(int));
	if (new_managed_pids == NULL){
		free(new_managed_pids);
		terminate_all_children();
		handle_error("realloc failed");
	}
	managed_pids = new_managed_pids;
	int* new_pipe_read_fds = realloc(pipe_read_fds, actual_num_processes*sizeof(int));
	if (new_pipe_read_fds == NULL){
		free(new_pipe_read_fds);
		terminate_all_children();
		handle_error("realloc failed");
	}
	pipe_read_fds = new_pipe_read_fds;
}

void my_sigpipe_handler(int signum){
	printf("SIGPIPE for Manager procecc %d. Leaving.", getpid());
	terminate_all_children();
	free(managed_pids);
	int j;
	for(j = 0; j < actual_num_processes; j++){
		if (pipe_read_fds[j] != -1)
			close(pipe_read_fds[j]);
	}
	free(pipe_read_fds);
	exit(EXIT_SUCCESS);
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

int main(int argc, char** argv){
	//assert number of arguments 
	assert(argc == 3);
	register_sigpipe();
	//initialization
	actual_num_processes = strlen(argv[2]);
	managed_pids = (int*) malloc(actual_num_processes*sizeof(int));
	if (managed_pids == NULL)
		handle_error("Memory allocation failure");
	pipe_read_fds = (int*) malloc(actual_num_processes*sizeof(int));
	if (pipe_read_fds == NULL)
		handle_error("Memory allocation failure");
 	initialize_processes(argv[1], argv[2]);
	sleep(1);
	//iterate throught the launched processes
	int stat_loc = -1;
	int keep_running = 1;
	int i;
	while(keep_running){
		for (i = 0; i < actual_num_processes; i++){
			if (waitpid(managed_pids[i], &stat_loc, WNOHANG) == -1){
				terminate_all_children();
				handle_error("waitpid failed");
			}
			if (WIFEXITED(stat_loc)){
				if (stat_loc == EXIT_SUCCESS){
					//print the data reported by the process
					char write_buffer;
					while(read(pipe_read_fds[i], &write_buffer, 1 ) > 0 )
					{
						printf( "%c", write_buffer );
					}
					
					close(pipe_read_fds[i]);
				}
				//exclude the process
				actual_num_processes--;
				if (actual_num_processes != 0)
					exclude_process_from_list(i);		
			}
		}
		if (actual_num_processes == 0)
			keep_running = 0;
		sleep(1);
	}

	free(managed_pids);
	free(pipe_read_fds);
	return EXIT_SUCCESS;


}
