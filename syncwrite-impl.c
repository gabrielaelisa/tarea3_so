/* Necessary includes for device drivers */
#include <linux/init.h>
/* #include <linux/config.h> */
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */

#include "kmutex.h"

MODULE_LICENSE("Dual BSD/GPL");

/* Declaration of syncread.c functions */
int syncread_open(struct inode *inode, struct file *filp);
int syncread_release(struct inode *inode, struct file *filp);
ssize_t syncread_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t syncread_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void syncread_exit(void);
int syncread_init(void);

/* Structure that declares the usual file */
/* access functions */
struct file_operations syncread_fops = {
  read: syncread_read,
  write: syncread_write,
  open: syncread_open,
  release: syncread_release
};

/* Declaration of the init and exit functions */
module_init(syncread_init);
module_exit(syncread_exit);

/*** El driver para lecturas sincronas *************************************/

#define TRUE 1
#define FALSE 0