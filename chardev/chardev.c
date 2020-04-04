#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>


/*
 *  Prototypes
 */
static int myinit(void);

static void myexit(void);

static void cleanup(int device_created);

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

static int opened = 0;
static long writes_count = 0;

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
    if (opened) {
        printk(KERN_ALERT
        "Wait, release it first.\n");
        return;
    }
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
    if (opened) {
        printk(KERN_ALERT
        "Fuck off, it's been already opened.\n");
    }
    opened = 1;
    return SUCCESS;
}

/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO
    "Releasing.\n");
    opened = 0;
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
    char message[MAX_STRING_LEN] = {0};
    int err = SUCCESS;
    size_t message_size = 0;

    printk(KERN_INFO
    "Preparing to answer to 'read'.\n");

    sprintf(message, "I've counted %ld writes.\n", writes_count);
    message_size = strlen(message);

    err = copy_to_user(buffer, message, message_size);

    if (err != SUCCESS) {
        printk(KERN_ALERT
        "Failed to answer.\n");
        return -EFAULT;
    }

    printk(KERN_INFO
    "Answered to the 'read' request.\n");
    return message_size;
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/hello
 */
static ssize_t device_write(struct file *filp, const char *buffer, size_t len, loff_t *offset) {
    char message[MAX_STRING_LEN] = {0};
    unsigned long err = SUCCESS;

    if (len >= MAX_STRING_LEN) {
        printk(KERN_ALERT
        "Too much for me\n");
        return -EFAULT;
    }

    err = copy_from_user(message, buffer, len);
    if (err != SUCCESS) {
        printk(KERN_ALERT
        "Failed to recieve user's message.\n");
        return -EFAULT;
    }

    printk(KERN_INFO
    "Here's what I've got: '%s'\n", message);

    writes_count++;
    return len;
}

module_init(myinit)
module_exit(myexit)
MODULE_LICENSE("GPL");
