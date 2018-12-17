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

int syncwrite_major = 65;     /* Major number */

/* Buffer to store data */
#define MAX_SIZE 8192
#define MAXDEVICES 2

// variables globales

static char *syncwrite_buffer;
static ssize_t size;
static int readers;
static int writing;

/* El mutex y la noreadericion para syncwrite */
static KMutex mutex;
// todos los escritores esperan un lector
static KCondition noreader;


int syncwrite_init(void) {
  int rc;

  /* Registering device */
  rc = register_chrdev(syncwrite_major, "syncwrite", &syncwrite_fops);
  if (rc <0) {
    printk(
      "<1>syncwrite: cannot obtain major number %d\n", syncwrite_major);
    return rc;
  }

  readers= FALSE;
  writing= 0;
  size=0;
  m_init(&mutex);
  c_init(&noreader);

  /* Allocating syncwrite_buffer */
  syncwrite_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
  if (syncwrite_buffer==NULL) {
    syncwrite_exit();
    return -ENOMEM;
  }
  memset(syncwrite_buffer, 0, MAX_SIZE);

  printk("<1>Inserting syncwrite module\n");
  return 0;
}


void syncwrite_exit(void) {
  /* Freeing the major number */
  unregister_chrdev(syncwrite_major, "syncwrite");

  /* Freeing buffer syncwrite */
  if (syncwrite_buffer) {
    kfree(syncwrite_buffer);
  }

  printk("<1>Removing syncwrite module\n");
}


int syncwrite_open(struct inode *inode, struct file *filp) {
  int rc= 0;
  m_lock(&mutex);

  // escritura
  if (filp->f_mode & FMODE_WRITE) {
    writing+= 1;
    printk("<1>open for write successful\n");
  }

  // lectura
  else if (filp->f_mode & FMODE_READ) {
    printk("<1> here looping\n");
    if(writing==0)
    {
      printk("<1>no writers\n");
      //rc= -EINTR;
      goto epilog;
    }

    readers= TRUE;
    c_broadcast(&noreader);
    printk("<1>open for read\n");
    
  }

epilog:
  m_unlock(&mutex);
  return rc;
}

int syncwrite_release(struct inode *inode, struct file *filp) {
  m_lock(&mutex);

  if (filp->f_mode & FMODE_WRITE) {
    writing--;
    printk("<1>close for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {
    size=0;
    readers= FALSE;
    printk("<1>close for read (readers remaining=%d)\n", readers);
  }

  m_unlock(&mutex);
  return 0;
}

// esta parte hay que editar y cambiar
ssize_t syncwrite_read(struct file *filp, char *buf,
                    size_t count, loff_t *f_pos) {
  ssize_t rc;
  m_lock(&mutex);

  if (count > size-*f_pos) {
    count= size-*f_pos;
  }

  printk("<1>read %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos hacia el espacio del usuario */
  if (copy_to_user(buf, syncwrite_buffer+*f_pos, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }

  *f_pos+= count;
  rc= count;

epilog:
  m_unlock(&mutex);
  return rc;
}




ssize_t syncwrite_write( struct file *filp, const char *buf,
                      size_t count, loff_t *f_pos) {
  
  //minor number
  int minor= iminor(filp->f_path.dentry->d_inode);
  ssize_t rc;
  loff_t last;
  m_lock(&mutex);

  last= size + count;
  if (last>MAX_SIZE) {
      count -= last-MAX_SIZE;
    }

  // priority write 
  if(minor==1){
    // creating auxiliary buffer
    char * aux_buffer = kmalloc(size+1, GFP_KERNEL);
    //error during creation
    if (aux_buffer==NULL) {
      writing--;
      printk("could not make buffer");
      kfree(aux_buffer);
      return -ENOMEM;
    }
    //copy current values
    memcpy(aux_buffer,syncwrite_buffer, sizeof(size));
    
    if (copy_from_user(syncwrite_buffer, buf, count)!=0) {
      writing--;
      rc= -EFAULT;
      goto epilog;
    }

    memcpy(syncwrite_buffer+count, aux_buffer, sizeof(size));
    kfree(aux_buffer);

  }


  // no priority
  else if(minor==0){

    printk("<1>write %d bytes at %d\n", (int)count, (int)*f_pos);

    /* Transfiriendo datos desde el espacio del usuario*/ 
    if (copy_from_user(syncwrite_buffer+size, buf, count)!=0) {
      /* el valor de buf es una direccion invalida*/
      writing--;
      rc= -EFAULT;
      goto epilog;
    }

  }
  size+=count;
  *f_pos += count;
  rc= count;
  while(!readers){
    if (c_wait(&noreader, &mutex)) {
      writing--;
      rc= -EINTR;
      goto epilog;
    }
  }

 

epilog:
    m_unlock(&mutex);
    return rc;
  }




/*/----------------------------------------------------------
ssize_t syncwrite_write( struct file *filp, const char *buf,
                      size_t count, loff_t *f_pos) {
  
  // minor number
  //int minor= iminor(filp->f_path.dentry->d_inode);
  ssize_t rc;
  loff_t last;

  m_lock(&mutex);

  last= size + count;
  if (last>MAX_SIZE) {
    count -= last-MAX_SIZE;
  }
  printk("<1>write %d bytes at %d\n", (int)count, (int)*f_pos);

  if (copy_from_user(syncwrite_buffer+size, buf, count)!=0) {
    rc= -EFAULT;
    goto epilog;
  }
  size+=count;
  rc= count;
  while(!readers){
    if (c_wait(&noreader, &mutex)) {
      writing--;
      rc= -EINTR;
      goto epilog;
      }

  }

epilog:
  m_unlock(&mutex);
  return rc;
}
*/
