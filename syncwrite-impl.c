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

int syncwrite_major = 61;     /* Major number */

/* Buffer to store data */
#define MAX_SIZE 8192

// variables globales

static char *syncwrite_buffer;
static ssize_t curr_size;
static int readers;
static int writing;

/* El mutex y la noreadericion para syncwrite */
static KMutex mutex;
// todos los escritores esperan un lector
static Knoreaderition noreader;

int syncwrite_init(void) {
  int rc;

  /* Registering device */
  rc = register_chrdev(syncwrite_major, "syncwrite", &syncwrite_fops);
  if (rc < 0) {
    printk(
      "<1>syncwrite: cannot obtain major number %d\n", syncwrite_major);
    return rc;
  }

  readers= FALSE;
  writing= 0;
  curr_size= 0;
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

  // minor number
  int minor= iminor(filp->f_path.dentry->d_inode)
  // escritura
  if (filp->f_mode & FMODE_WRITE) {
    int rc;
    printk("<1>open request for write\n");
    /* Se debe esperar hasta que no hayan otros lectores o escritores */
    while (!readers) {
      // si hay control-c entro al error
      if (c_wait(&noreader, &<mutex)) {
        rc= -EINTR;
        goto epilog;
      }
    }
    writing= TRUE;
    curr_size= 0;
    printk("<1>open for write successful\n");
  }

  // lectura
  else if (filp->f_mode & FMODE_READ) {

    while (!writing && pend_open_write>0) {
      if (c_wait(&noreader, &mutex)) {
        rc= -EINTR;
        goto epilog;
      }
    }
    readers++;
    printk("<1>open for read\n");
  }

epilog:
  m_unlock(&mutex);
  return rc;
}

int syncwrite_release(struct inode *inode, struct file *filp) {
  m_lock(&mutex);

  if (filp->f_mode & FMODE_WRITE) {
    writing= FALSE;
    c_broadcast(&noreader);
    printk("<1>close for write successful\n");
  }
  else if (filp->f_mode & FMODE_READ) {
    readers--;
    if (readers==0)
      c_broadcast(&noreader);
    printk("<1>close for read (readers remaining=%d)\n", readers);
  }

  m_unlock(&mutex);
  return 0;
}

// esta parte hay que editar y cambiar
ssize_t syncread_read(struct file *filp, char *buf,
                    size_t count, loff_t *f_pos) {
  ssize_t rc;
  m_lock(&mutex);

  while (curr_size <= *f_pos && writing) {
    /* si el lector esta en el final del archivo pero hay un proceso
     * escribiendo todavia en el archivo, el lector espera.
     */
    if (c_wait(&noreader, &mutex)) {
      printk("<1>read interrupted\n");
      rc= -EINTR;
      goto epilog;
    }
  }

  if (count > curr_size-*f_pos) {
    count= curr_size-*f_pos;
  }

  printk("<1>read %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos hacia el espacio del usuario */
  if (copy_to_user(buf, syncread_buffer+*f_pos, count)!=0) {
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

ssize_t syncread_write( struct file *filp, const char *buf,
                      size_t count, loff_t *f_pos) {
  ssize_t rc;
  loff_t last;

  m_lock(&mutex);

  last= *f_pos + count;
  if (last>MAX_SIZE) {
    count -= last-MAX_SIZE;
  }
  printk("<1>write %d bytes at %d\n", (int)count, (int)*f_pos);

  /* Transfiriendo datos desde el espacio del usuario */
  if (copy_from_user(syncread_buffer+*f_pos, buf, count)!=0) {
    /* el valor de buf es una direccion invalida */
    rc= -EFAULT;
    goto epilog;
  }

  *f_pos += count;
  curr_size= *f_pos;
  rc= count;
  c_broadcast(&noreader);

epilog:
  m_unlock(&mutex);
  return rc;
}

