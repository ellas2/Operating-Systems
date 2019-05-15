// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE
#include <linux/slab.h>		/* kmalloc()*/
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>    /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/types.h>
#include "message_slot.h"
MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations

typedef struct 
{
	int minor_num;
	char messages[NUM_CHANNELS][MESSAGE_MAX_LEN];
	int actual_length[NUM_CHANNELS];
} message_slot_info;

static int num_slots = 0;
static message_slot_info* slots = NULL;

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
	int i;
	int j;
	bool minor_exists;
	int curr_minor_num;
	minor_exists = false;
	curr_minor_num = iminor(inode);
	for (i = 0; i < num_slots; i++)
	{
		if (slots[i].minor_num == curr_minor_num)
		{
			minor_exists = true;
			break;
		}
	}
	if (!minor_exists)
	{
		if (num_slots == 0) 
		{

			slots = (message_slot_info*)kmalloc(sizeof(message_slot_info), GFP_KERNEL);
			if (slots == NULL)
			{
				printk(KERN_ALERT "kmalloc failed\n");
				return -ENOMEM;
			}
			memset(slots, 0, sizeof(message_slot_info));
		}
		else
		{		
			message_slot_info* new_slots = krealloc(slots,(num_slots+1)*sizeof(message_slot_info), GFP_KERNEL);
			if (new_slots == NULL)
			{
				printk(KERN_ALERT "krealloc failed\n");	
				kfree(slots);
				return -ENOMEM;	
			}
			slots = new_slots;
			memset(&(slots[num_slots]), 0, sizeof(message_slot_info));
			
		}
		slots[num_slots].minor_num = curr_minor_num;
		num_slots++;
		for (j = 0; j < NUM_CHANNELS; j++)
		{
			slots[num_slots].actual_length[j] = 0;
		}
	}
	
	file->private_data = (void*)(NUM_CHANNELS);
	return SUCCESS;
}

//---------------------------------------------------------------
static int device_release( struct inode* inode,
                           struct file*  file)
{
	return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
	int i;
	int curr_channel;
	int curr_minor;
	int j;
	int len;
	len = (int) length;
	curr_channel = (uintptr_t)file->private_data;
	if (curr_channel == NUM_CHANNELS)
		return -EINVAL;
	curr_minor = iminor(file_inode(file));
	for (i = 0; i < num_slots; i++)
	{
		if (slots[i].minor_num == curr_minor)
			break;
	}
	if (i == num_slots)
		return -EINVAL;//minot doesn't exist
	if (slots[i].actual_length[curr_channel] == 0 || slots[i].messages[curr_channel] == NULL) 
		return -EWOULDBLOCK;
	if (len < slots[i].actual_length[curr_channel])
		return -ENOSPC;
	for(j = 0; j < slots[i].actual_length[curr_channel]; ++j)
	{
		if (put_user(slots[i].messages[curr_channel][j], &buffer[j]) != 0)
		{
			printk(KERN_ALERT "put_user failed\n");
			return -EINVAL;
		}
	}
 	return j;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
	int curr_channel;
	int i;
	int j;
	int len;
	int curr_minor;
	len = (int) length;
	curr_channel = (uintptr_t)file->private_data;
	if (curr_channel == NUM_CHANNELS)
		return -EINVAL;
	if (len == 0)
		return -EINVAL;
	if (buffer == NULL)
		return -EINVAL;
	if (len > MESSAGE_MAX_LEN)
		return -EINVAL;
	curr_minor = iminor(file_inode(file));
	for (i = 0; i < num_slots; i++)
	{
		if (slots[i].minor_num == curr_minor)
			break;
	}
	if (i == num_slots)
		return -EINVAL;
	for( j = 0; j < len; ++j )
	{
		if (get_user(slots[i].messages[curr_channel][j], &buffer[j]) != 0)
		{
			printk(KERN_ALERT "get_user failed\n");
			return -EINVAL;
		}
	}
	slots[i].actual_length[curr_channel] = len;
	return j;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
  	if (ioctl_command_id != MSG_SLOT_CHANNEL)
		return -EINVAL;
	if (!(ioctl_param >= 0 && ioctl_param <=3))//channel not in range
		return -EINVAL;
	file->private_data = (void*)ioctl_param;
  return SUCCESS;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops =
{
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
	int rc = -1;
	rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
	if (rc < 0)
	{
		printk( KERN_ALERT "registraion failed for  %d\n", MAJOR_NUM );
		return rc;
	}

	return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
	kfree(slots);
	//Unregister the device
	//Should always succeed
 	unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
