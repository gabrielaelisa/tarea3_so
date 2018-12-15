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

/* Declaration of syncwrite0.c functions */
int syncwrite_open(struct inode *inode, struct file *filp);
int syncwrite_release(struct inode *inode, struct file *filp);
ssize_t syncwrite_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t syncwrite_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
void syncwrite_exit(void);
int syncwrite_init(void);



/* Structure that declares the usual file */
/* access functions */
struct file_operations syncwrite_fops = {
  read: syncwrite_read,
  write: syncwrite_write,
  open: syncwrite_open,
  release: syncwrite_release
};

/* Declaration of the init and exit functions */
module_init(syncwrite_init);
module_exit(syncwrite_exit);

/*** El driver para lecturas sincronas *************************************/

#define TRUE 1
#define FALSE 0

/* Global variables of the driver */

int syncread_major = 61;     /* Major number */

/* Buffer to store data */
#define MAX_SIZE 8192

// variables globales

static char *syncwrite_buffer;
static ssize_t curr_size;
static int readers;
static int writing;
static int pend_open_write;

/* El mutex y la condicion para syncread */
static KMutex mutex;
static KCondition cond;

int syncwrite_init(void) {
  int rc;

  /* Registering device */
  rc = register_chrdev(syncread_major, "syncread", &syncwrite_fops);
  if (rc < 0) {
    printk(
      "<1>syncread: cannot obtain major number %d\n", syncwrite_major);
    return rc;
  }

  readers= 0;
  writing= FALSE;
  pend_open_write= 0;
  curr_size= 0;
  m_init(&mutex);
  c_init(&cond);

  /* Allocating syncread_buffer */
  syncwrite_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
  if (syncwrite_buffer==NULL) {
    syncread_exit();
    return -ENOMEM;
  }
  memset(syncwrite_buffer, 0, MAX_SIZE);

  printk("<1>Inserting syncread module\n");
  return 0;
}

void syncwrite_exit(void) {
  /* Freeing the major number */
  unregister_chrdev(syncwrite_major, "syncread");

  /* Freeing buffer syncread */
  if (syncwrite_buffer) {
    kfree(syncwrite_buffer);
  }

  printk("<1>Removing syncread module\n");
}
