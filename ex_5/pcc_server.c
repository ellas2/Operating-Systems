/*
 * pcc_server.c
 *
 *  Created on: Jun 12, 2018
 *      Author: ellas2
 */
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>



#define UPPER_P 126
#define LOWER_P 32
#define NUM_P ((UPPER_P-LOWER_P)+1)
#define CONNECTION_QUEUE_SIZE 100

int connect_fd;
char pcc_count[NUM_P];
pthread_t* threads = NULL;
int size_threads;
bool should_accept = true;
pthread_mutex_t pcc_count_lock;


void handle_error(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void print_pcc_count()
{
	int i;
	for (i = 0; i < NUM_P; i++)
	{
		printf("char '%c' : %u times\n", (char)(i+LOWER_P), pcc_count[i]);
	}
}

void update_pcc_count(char c)
{
	int ind = (int)c-LOWER_P ;
	if (ind >= 0 && ind < NUM_P)
		pcc_count[ind] ++;
}

void enlarge_threads_by_one()
{
	pthread_t* new_threads = (pthread_t*)realloc(threads, (size_threads+1)*sizeof(pthread_t));
	if (new_threads == NULL)
		handle_error("Failed reallocating memory\n");
	threads = new_threads;
	size_threads++;
}

void send_num_printables_to_client(int num_printables)
{
	int bytes_to_send_n, bytes_sent_n;
	char* num_string;
	uint32_t converted_num;

	converted_num = htonl(num_printables);
	num_string = (char*) &converted_num;
	bytes_to_send_n = sizeof(num_string);
	while (bytes_to_send_n > 0) {
		if ((bytes_sent_n = write(connect_fd, num_string, bytes_to_send_n))
				== -1)
			handle_error("Failed writing to socket fd\n");
		bytes_to_send_n -= bytes_sent_n;
		num_string += bytes_sent_n;
	}
}

void read_msg_from_client(char* buff, int msg_length)
{
	int bytes_to_read_str, bytes_read_str;

	bytes_to_read_str = msg_length;
	bytes_read_str = 0;
	while(bytes_to_read_str > 0)
	{
		if ((bytes_read_str = read(connect_fd, buff, bytes_to_read_str)) == -1)
			handle_error("Failed reading from socket fd\n");
		buff += bytes_read_str;
		bytes_to_read_str -= bytes_read_str;
	}
}

unsigned int read_num_chars()
{
	int msg_length;
	int32_t c_helper;
	int bytes_to_read_c, bytes_read_c;
	char* c_string;

	c_string = (char*) &c_helper;
	bytes_to_read_c = sizeof(c_helper);

	while (bytes_to_read_c > 0) {
		if ((bytes_read_c = read(connect_fd, c_string, bytes_to_read_c)) == -1)
			handle_error("Failed reading from socket fd\n");
		c_string += bytes_read_c;
		bytes_to_read_c -= bytes_read_c;
	}
	msg_length = ntohl(c_helper);
	return msg_length;
}

void* communicate_with_client(void *t)
{
	char* read_from_socket = NULL;
	int num_printables = 0, i;
	unsigned int msg_length;
	msg_length = read_num_chars();
	read_from_socket = (char*)malloc(msg_length*sizeof(char));
	read_msg_from_client(read_from_socket, msg_length);
	//go over the local array - calc the num of printables and update the global array
	if (pthread_mutex_lock(&pcc_count_lock) != 0)
		handle_error("Failed locking mutex\n");
	for (i = 0; i < msg_length; i++)
	{
		int ascii_val = (int) read_from_socket[i];
		if (ascii_val >= LOWER_P && ascii_val <= UPPER_P)
		{
			num_printables++;
			update_pcc_count(read_from_socket[i]);
		}
	}
	if (pthread_mutex_unlock(&pcc_count_lock) != 0)
		handle_error("Failed unlocking mutex\n");
	send_num_printables_to_client(num_printables);
	pthread_exit(NULL);
}

void my_sigint_handler(int signum)
{
	int i;
	should_accept = false;
	for (i = 0; i < size_threads; i++)
	{
		if (pthread_join(threads[i], NULL) != 0)
			handle_error("Failed to join threads\n");
	}
	print_pcc_count();
	exit(EXIT_SUCCESS);
}

void register_sigint()
{
	struct sigaction my_sigint_action;
	memset(&my_sigint_action, 0, sizeof(my_sigint_action));
	// Assign pointer to our handler function
	my_sigint_action.sa_handler = my_sigint_handler;
	// Register the handler
	if(sigaction(SIGINT, &my_sigint_action, NULL) != 0){
		handle_error("Failed to register SIGINT handler\n");
	}
}

int main(int argc, char *argv[])
{
	int listen_fd, reuse;
	uint16_t server_port;
	struct sockaddr_in serv_addr;
	register_sigint();
	memset(pcc_count, 0, sizeof(char)*NUM_P);
	server_port = strtoul(argv[1], NULL, 10);
	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			handle_error("Failed creating socket\n");
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(server_port);
	 if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) == -1)
		 handle_error("Failed binding socket\n");
	 if (listen(listen_fd, CONNECTION_QUEUE_SIZE) == -1)
		 handle_error("Failed to start listening to incoming connections\n");
	// allow reuse of socket
	reuse = 1;
	if ( setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
		handle_error("Failed to set socket reuseadd\n");

	pthread_mutex_init(&pcc_count_lock, NULL);

	struct sockaddr_in client_addr;
	socklen_t client_addr_size = sizeof(struct sockaddr_in);
	size_threads = 0;
	while (1)
	{
		 struct sockaddr_in client_addr;
		 socklen_t client_addr_size = sizeof(struct sockaddr_in);
		 if ((connect_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_size)) == -1)
			 handle_error("Failed accepting connection\n");
		 if (should_accept == true)
		 {
			 enlarge_threads_by_one();
			 if (pthread_create(&threads[size_threads-1], NULL, communicate_with_client, (void*)(long)connect_fd) != 0)
				 handle_error("Failed creating thread\n");
		 }
		 else
			 close(connect_fd);

	}
}
