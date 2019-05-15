#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define BUF_SIZE 1048576
#define EXIT_SUCCESS 0 

typedef struct
{
	int signaled_in_last_stage;
	int* finished_curr_stage;
	int num_stages;
	int curr_stage;
} thread_info;

int global_stage = 0;
thread_info* thread_infos = NULL;
int num_input_files;
pthread_t *threads = NULL;
pthread_mutex_t xor_lock;
char write_buf[BUF_SIZE];
int out_fd;
int* in_fds = NULL;
pthread_cond_t can_xor;
int* can_write = NULL;
int curr_buf_len = 0;
int out_size = 0;
int global_num_stages = 0;

void handle_error(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

//a function that returns 1 iff j is the last thread in stage stage_num
int last_thread(int j, int stage_num)
{
	int i;
	for (i = 0; i < num_input_files; i++)
	{
		int i_num_stages = thread_infos[i].num_stages;
		if (i != j && i_num_stages > stage_num && thread_infos[i].finished_curr_stage[stage_num] == 0)
		{
			return 0;
		}
	}
	return 1;
}

//the thread function
void *read_and_xor (void *t)
{
	int i;
	char* read_buf = (char*)malloc(BUF_SIZE * sizeof(char));
	if (read_buf == NULL)
		handle_error("ERROR in malloc\n");
	long my_id = (long)t;
	while (thread_infos[my_id].curr_stage < thread_infos[my_id].num_stages)
	{
		//read concurrently without locking
		int bytes_read, bytes_written;
		bytes_read = read(in_fds[my_id], read_buf, BUF_SIZE);
		if (bytes_read == -1)
			handle_error("ERROR in read\n");
		//want to write only if a signal has been sent
		if (pthread_mutex_lock(&xor_lock) != 0)
			handle_error("ERROR in pthread_mutex_lock\n");
		if (thread_infos[my_id].curr_stage > 0)//we don't want to wait for a signal in stage 0 (first stage)
		{
			while (can_write[my_id] == 0 || global_stage < thread_infos[my_id].curr_stage)
			{
				if (thread_infos[my_id].signaled_in_last_stage == 0)//if I signaled I might not get the signal
				{
					if (pthread_cond_wait(&can_xor, &xor_lock)!= 0)
						handle_error("ERROR in pthread_cond_wait\n");
				}
				else
					thread_infos[my_id].signaled_in_last_stage = 0;//so that it will wait next time
			}
		}
		//we want to know how many bytes to write to o/p
		if (curr_buf_len < bytes_read)
			curr_buf_len = bytes_read;
		can_write[my_id] = 0;
		//xoring!
		for (i = 0; i < bytes_read; i++)
		{
			write_buf[i] = (char)(write_buf[i] ^ read_buf[i]);
		}
		//write to o/p if I am the last thread and broadcast that stage is done
		if (last_thread(my_id, thread_infos[my_id].curr_stage) == 1)
		{
			bytes_written = write(out_fd, write_buf, curr_buf_len);
			if (bytes_written == -1)
				handle_error("ERROR in write\n");
			out_size += bytes_written;
			global_stage++;
			memset(write_buf, 0, BUF_SIZE);
			for (i = 0; i < num_input_files; i++)
			{
				can_write[i] = 1;
			}
			curr_buf_len = 0;
			thread_infos[my_id].signaled_in_last_stage = 1;
			if (pthread_cond_broadcast(&can_xor) != 0)
				handle_error("ERROR in pthread_cond_broadcast\n");
		}
		thread_infos[my_id].finished_curr_stage[thread_infos[my_id].curr_stage] = 1;
		thread_infos[my_id].curr_stage++;
		if (pthread_mutex_unlock(&xor_lock) != 0)
			handle_error("ERROR in pthread_mutex_unlock\n");
	}
	free(read_buf);
	pthread_exit(NULL);
}

int ceiling (int a, int b) {
	if (a % b == 0)
		return a / b;
	else
		return (a / b + 1);
}

int main (int argc, char *argv[])
{
	int i;
	num_input_files = argc - 2;
	printf("Hello, creating %s from %d input files\n", argv[1], num_input_files);
	can_write = (int*)malloc(num_input_files * sizeof(int));
	if (can_write == NULL)
		handle_error("ERROR in malloc\n");
	threads = (pthread_t*)malloc(num_input_files * sizeof(pthread_t));
	if (threads == NULL)
		handle_error("ERROR in malloc\n");
	in_fds = (int*)malloc(num_input_files * sizeof(int));
	if (in_fds == NULL)
		handle_error("ERROR in malloc\n");
	thread_infos = (thread_info*)malloc(num_input_files * sizeof(thread_info));
	if (thread_infos == NULL)
		handle_error("ERROR in malloc\n");
	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0)
		handle_error("ERROR in pthread_attr_init\n");
	if (pthread_cond_init(&can_xor, NULL) != 0)
		handle_error("ERROR in pthread_cond_init\n");
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0)
		handle_error("ERROR in pthread_attr_setdetachstate\n");
	if (pthread_mutex_init(&xor_lock, NULL) != 0)
		handle_error("ERROR in pthread_mutex_init\n");
	memset(write_buf, 0 , BUF_SIZE);
	out_fd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (out_fd == -1)
		handle_error("ERROR in open\n");
	//so that threads can write in the first stage
	for (i = 0; i < num_input_files; i++)
	{
		can_write[i] = 1;
	}
	//create a reader thread for each input
	for (i = 0; i < num_input_files; i++)
	{
		in_fds[i] = open(argv[i + 2], O_RDONLY);
		if (in_fds[i] == -1)
			handle_error("ERROR in open\n");
		struct stat st;
		if (stat(argv[i + 2], &st) == -1)
			handle_error("ERROR in stat\n");
		thread_infos[i].num_stages = ceiling(st.st_size, BUF_SIZE);
		if (global_num_stages < thread_infos[i].num_stages)
			global_num_stages = thread_infos[i].num_stages;
		thread_infos[i].curr_stage = 0;
		thread_infos[i].finished_curr_stage = (int*)malloc((thread_infos[i].num_stages) * sizeof(int));
		memset(thread_infos[i].finished_curr_stage, 0, thread_infos[i].num_stages * sizeof(int));
		thread_infos[i].signaled_in_last_stage = 0;
		if (pthread_create(&threads[i], &attr, read_and_xor, (void*)(long)i) != 0)
			handle_error("ERROR in pthread_create\n");

	}
	//wait for all reader threads to finish
	for (i = 0; i < num_input_files; i++)
	{
		if (pthread_join(threads[i], NULL) != 0)
			handle_error("ERROR in pthread_join\n");
	}
	//close output file
	close(out_fd);
	printf("Created %s with size %d bytes\n", argv[1], out_size);
	//exit with exit code 0
	for (i = 0; i < num_input_files; i++)
	{
		close(in_fds[i]);
	}
	if (pthread_attr_destroy(&attr) != 0)
		handle_error("ERROR in pthread_attr_destroy\n");
	if (pthread_mutex_destroy(&xor_lock) != 0)
		handle_error("ERROR in pthread_mutex_destroy\n");
	if (pthread_cond_destroy(&can_xor) != 0)
		handle_error("ERROR in pthread_cond_destroy\n");
	free(threads);
	free(in_fds);
	for (i = 0 ; i < num_input_files; i++)
	{
		free(thread_infos[i].finished_curr_stage);
	}
	free(thread_infos);
	free(can_write);
	exit(EXIT_SUCCESS);
}

