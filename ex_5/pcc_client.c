/*
 * pcc_client.c
 *
 *  Created on: Jun 10, 2018
 *      Author: ellas2
 */
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

int sock_fd;
char* read_from_urand = NULL;
unsigned int length, c;

void handle_error(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

bool is_valid_ip(char* str_ip) {
	struct sockaddr_in sa;
	int result;
	if ((result = inet_pton(AF_INET, str_ip, &(sa.sin_addr))) == 1)
		return true;
	//not a valid address in ipv4 or not a valid address at all
	else
		return false;
}

void get_c_from_server()
{
	uint32_t c_helper;
	char* c_string;
	int  bytes_read_c, bytes_to_read_c;
	c_string = (char*) &c_helper;
	bytes_to_read_c = sizeof(c_helper);
	while (bytes_to_read_c > 0) {
		if ((bytes_read_c = read(sock_fd, c_string, bytes_to_read_c)) == -1)
			handle_error("Failed reading from socket fd\n");
		c_string += bytes_read_c;
		bytes_to_read_c -= bytes_read_c;
	}
	c = ntohl(c_helper);
}

void send_msg_to_server()
{
	int bytes_to_send, bytes_sent;
	char* temp_read_from_urand;
	bytes_to_send = length;
	bytes_sent = 0;
	temp_read_from_urand = read_from_urand;
	while (bytes_to_send > 0) {
		if ((bytes_sent = write(sock_fd, temp_read_from_urand, bytes_to_send)) == -1)
			handle_error("Failed writing to socket fd\n");
		bytes_to_send -= bytes_sent;
		temp_read_from_urand += bytes_sent;
	}
}
void send_length_to_server()
{
	int bytes_sent_l, bytes_to_send_l;
	uint32_t converted_length;
	char* length_string;

	converted_length = htonl(length);
	length_string = (char*) &converted_length;
	bytes_to_send_l = sizeof(length_string);
	while (bytes_to_send_l > 0) {
		if ((bytes_sent_l = write(sock_fd, length_string, bytes_to_send_l))
				== -1)
			handle_error("Failed writing to socket fd\n");
		bytes_to_send_l -= bytes_sent_l;
		length_string += bytes_sent_l;
	}
}

void open_and_read_from_urand()
{
	int urandom_fd, bytes_to_read, bytes_read;
	char* temp_read_from_urand;

	if ((urandom_fd = open("/dev/urandom", O_RDONLY)) == -1)
		handle_error("Failed opening /dev/urandom\n");
	bytes_to_read = length;
	bytes_read = 0;
	temp_read_from_urand = read_from_urand;
	while (bytes_to_read > 0) {
		if ((bytes_read = read(urandom_fd, temp_read_from_urand, bytes_to_read)) == -1)
			handle_error("Failed reading from /dev/urandom\n");
		bytes_to_read -= bytes_read;
		temp_read_from_urand += bytes_read;

	}
}
int main(int argc, char *argv[]) {
	uint16_t server_port;
	struct addrinfo hints;
	struct addrinfo *server_info, *p;
	struct sockaddr_in serv_addr;
	memset(&hints, 0, sizeof(struct addrinfo));
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	server_port = strtoul(argv[2], NULL, 10);
	length = strtoul(argv[3], NULL, 10);
	read_from_urand = (char*) malloc(length * sizeof(char));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port);

	//create a tcp connection
	if (is_valid_ip(argv[1])) {
		if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			handle_error("Failed creating socket\n");
		if ((inet_aton(argv[1], &(serv_addr.sin_addr))) == 0)
			handle_error("Failed calling inet_aton\n");
		if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))
				== -1)
			handle_error("Failed connecting with ip as first argument\n");
	} else {
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		hints.ai_protocol = 0;
		if (getaddrinfo(argv[1], argv[2], &hints, &server_info) != 0) {
			handle_error("Failed calling getaddrinfo\n");
		}
		for (p = server_info; p != NULL; p = p->ai_next) {
			if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
					== -1)
				continue;
			if (connect(sock_fd, p->ai_addr, p->ai_addrlen) != -1)
				break;
			close(sock_fd);
		}
		if (p == NULL)
			handle_error("Failed connecting with hostname as first argument\n");
		freeaddrinfo(server_info);
	}
	open_and_read_from_urand();
	send_length_to_server();
	send_msg_to_server();
	get_c_from_server();
	printf("# of printable characters: %u\n", c);
	exit(EXIT_SUCCESS);
}
