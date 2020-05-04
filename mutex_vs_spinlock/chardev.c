#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>

/*
 *  Prototypes
 */
static int myinit(void);

static void myexit(void);

static void cleanup(int device_created);

static void increment_with_spinlock(void);

static void increment_with_mutex(void);

/* File operations */
static int device_open(struct inode *, struct file *);

static int device_release(struct inode *, struct file *);

static ssize_t device_read(struct file *, char *, size_t, loff_t *);

static ssize_t device_write(struct file *, const char *, size_t, loff_t *);


#define SUCCESS 0
#define FAIL -1
#define DEVICE_NAME "chardev"
#define MAX_STRING_LEN 256

/*
 * Global variables are declared as static, so are global within the file.
 */

static int Major;        /* Major number assigned to our device driver */

static struct cdev mycdev;
static struct class *myclass = NULL;

static struct file_operations fops = {
        read: device_read,
        write: device_write,
        open: device_open,
        release: device_release
};

/*
 * This function is called when the module is loaded.
 */
static int myinit(void) {
    int device_created = 0;

    /* cat /proc/devices */
    if (alloc_chrdev_region(&Major, 0, 1, DEVICE_NAME "_proc") < 0) {
        goto error;
    }

    printk(KERN_INFO
    "I was assigned major number %d.\n", Major);

    /* ls /sys/class */
    if ((myclass = class_create(THIS_MODULE, DEVICE_NAME "_sys")) == NULL) {
        goto error;
    }

    /* ls /dev/ */
    if (device_create(myclass, NULL, Major, NULL, DEVICE_NAME "_dev") == NULL) {
        goto error;
    }

    device_created = 1;

    cdev_init(&mycdev, &fops);
    if (cdev_add(&mycdev, Major, 1) == -1) {
        goto error;
    }
    return SUCCESS;

    error:
    cleanup(device_created);
    return FAIL;
}


static void cleanup(int device_created) {
    if (device_created) {
        device_destroy(myclass, Major);
        cdev_del(&mycdev);
    }

    if (myclass) {
        class_destroy(myclass);
    }

    if (Major != -1) {
        unregister_chrdev_region(Major, 1);
    }
}

/*
 * This function is called when the module is unloaded.
 */
static void myexit(void) {
    cleanup(1);
}

/*
 * Methods
 */

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO
    "Opening device.\n");
    return SUCCESS;
}

/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO
    "Releasing.\n");
    return SUCCESS;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,   /* see include/linux/fs.h   */
                           char *buffer,        /* buffer to fill with data */
                           size_t length,       /* length of the buffer     */
                           loff_t *offset) {

    printk(KERN_INFO "Read.\n");
    return 0;
}

#define N_THREADS 100
#define N_INCREMENTS 1000

static int shared_resource = 0;

DEFINE_SPINLOCK(sh_resource_lock);
unsigned long flags;

DEFINE_SEMAPHORE(sh_resource_mutex);

DECLARE_COMPLETION(comp);

static void increment_with_spinlock(void) {
  spin_lock_irqsave(&sh_resource_lock, flags);
  shared_resource++;
  spin_unlock_irqrestore(&sh_resource_lock, flags);
}

static void increment_with_mutex(void) {
  down(&sh_resource_mutex);
  shared_resource++;
  up(&sh_resource_mutex);
}

int thread_routine(void *arg) {
  int i;
  for (i = 0; i < N_INCREMENTS; i++) {
    increment_with_spinlock();
  }
  if (shared_resource == N_THREADS*N_INCREMENTS) {
    complete(&comp);
  }
  return SUCCESS;
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/hello
 */
static ssize_t device_write(struct file *filp, const char *buffer, size_t len, loff_t *offset) {
  int i;

  for (i = 0; i < N_THREADS; i++) {
    kthread_run(thread_routine, NULL, "my_thread%d", i);
  }
  wait_for_completion(&comp);
  printk(KERN_INFO "The shared resource == %d", shared_resource);
  shared_resource = 0;

  return len;
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");
