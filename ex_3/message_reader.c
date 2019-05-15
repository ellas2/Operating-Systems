#include "message_slot.h"    
#include <assert.h>
#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int main(int argc, char **argv)
{
	int fd;
	char* file_path;
	int channel_id;
	int bytes_read;
	char read_buf[MESSAGE_MAX_LEN+1];
	if (argc != 3)
	{
		printf("You should enter 3 arguments!\n");
		close(fd); 
		exit(ERROR);
	}
	file_path = argv[1];
	channel_id = atoi(argv[2]);
	//open the device file
	fd = open(file_path, O_RDONLY);
  	if(fd < 0) 
  	{
    		printf ("Can't open device file: %s\n", 
            file_path);
    		exit(ERROR);
  	}
	//set the channel id
	if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) == -1)
	{
		printf("ioctl failed!\n");
		close(fd); 
		exit(ERROR);
	}
	//read a message from the device with a buffer
	bytes_read = read(fd, read_buf, MESSAGE_MAX_LEN);
 	if (bytes_read == -1)
	{	
		printf("reading failed!\n");
		close(fd); 
		exit(ERROR);
	}
	read_buf[bytes_read] = '\0';
	close(fd);
	printf("%s\n", read_buf);
	printf("%d bytes read from %s\n", bytes_read,file_path); 
  return SUCCESS;
}
