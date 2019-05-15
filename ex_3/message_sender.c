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
	int msg_len;
	int bytes_written;
	if (argc != 4)
	{
		printf("You should enter 4 arguments!\n");
		close(fd); 
		exit(ERROR);
	}
	file_path = argv[1];
	channel_id = atoi(argv[2]);
	msg_len = strlen(argv[3]);
	//open the device file
	fd = open(file_path, O_RDWR);
  	if(fd < 0) 
  	{
    		printf ("Can't open device file: %s\n", file_path);
    		exit(ERROR);
  	}
	//set the channel id
	if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) == -1)
	{
		printf("ioctl failed!\n");
		close(fd); 
		exit(ERROR);
	}
	//write the specified message to the file
	bytes_written = write(fd, argv[3], msg_len);
	if (bytes_written == -1)
	{
		printf("write failed!\n");
		close(fd); 
		exit(ERROR);
	}
	close(fd); 
  return SUCCESS;
}
