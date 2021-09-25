#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bartosz Chwała");
MODULE_DESCRIPTION("RNG");
MODULE_VERSION("Proto");

/* Character device function signatures */
int init_MYDEV(void);
void cleanup_MYDEV(void);
static int open_MYDEV(struct inode *, struct file *);
static int release_MYDEV(struct inode *, struct file *);
static ssize_t read_MYDEV(struct file *, char *, size_t, loff_t *);
static ssize_t write_MYDEV(struct file *, const char *, size_t, loff_t *);

#define DEVICE_DEV_NAME "__prng_device" /* as it shows up in /dev/ etc. */
#define DEVICE_CLASS_NAME DEVICE_DEV_NAME
#define DEVICE_NAME_FORMAT DEVICE_DEV_NAME

#define RET_OK 0
#define RET_ERR (-1)
#define DEVICE_BUSY 1
#define DEVICE_FREE 0
#define STATE_BUF_LEN 4

static int device_state = DEVICE_FREE;
static char *prng_ptr;
static uint64_t STATE_BUF[STATE_BUF_LEN];

/* For module_init */
static dev_t dev;
static struct cdev c_dev;
static struct class* class_ptr;

static int mydev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static struct file_operations fops = {
        .read = read_MYDEV,
        .write = write_MYDEV,
        .open = open_MYDEV,
        .release = release_MYDEV
};

/* --       -- */

/* xoshiro256** impl as per
 * https://en.wikipedia.org/wiki/Xorshift */

uint64_t xos_rol64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

uint64_t xoshiro256ss(uint64_t* state_ptr) {
    uint64_t *s = state_ptr;
    uint64_t const result = xos_rol64(s[1] * 5, 7) * 9;
    uint64_t const t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = xos_rol64(s[3], 45);

    return result;
}

/* --       -- */


static void seed_MYDEV(void) {
    /* Incredible seed generator */
    uint64_t i;
    for (i = 0; i < STATE_BUF_LEN; ++i)
        STATE_BUF[i] = ktime_get_ns();
}

static void fill_rng_buffer_MYDEV(char ** buf_ptr, size_t length) {
    size_t i;
    char * helper_ptr = *buf_ptr;
    for(i = 0; i < length; i += sizeof(uint64_t)) {
        if(length - i < sizeof(uint64_t)) {
            uint64_t rn = xoshiro256ss(STATE_BUF);
            memcpy(&rn, helper_ptr, length - i);
        }
        *((uint64_t *) helper_ptr) = xoshiro256ss(STATE_BUF);
        helper_ptr += sizeof(uint64_t);
    }
}

int init_MYDEV(void)
{
    if ((alloc_chrdev_region(&dev, 0, 1, DEVICE_DEV_NAME)) < 0) {
        printk(KERN_INFO "Character device init failed at alloc_chrdev_region\n");
        return RET_ERR;
    }
    if((class_ptr = class_create(THIS_MODULE, DEVICE_CLASS_NAME)) == NULL) {
        printk(KERN_INFO "Character device init failed at class_create\n");
        unregister_chrdev_region(dev, 1);
        return RET_ERR;
    }
    class_ptr->dev_uevent = mydev_uevent;
    if(device_create(class_ptr, NULL, dev, NULL, DEVICE_NAME_FORMAT) == NULL) {
        printk(KERN_INFO "Character device init failed at device_create\n");
        class_destroy(class_ptr);
        unregister_chrdev(dev, DEVICE_DEV_NAME);
        return RET_ERR;
    }
    cdev_init(&c_dev, &fops);
    if(cdev_add(&c_dev, dev, 1) == -1) {
        printk(KERN_INFO "Character device init failed at cdev_add\n");
        device_destroy(class_ptr, dev);
        class_destroy(class_ptr);
        unregister_chrdev_region(dev, 1);
        return RET_ERR;
    }

    seed_MYDEV();
    printk(KERN_INFO "Character device registered successfully at /dev/%s\n", DEVICE_DEV_NAME);
    return RET_OK;
}

void cleanup_MYDEV(void)
{
    if(prng_ptr != NULL) {
        kfree(prng_ptr);
    }
    cdev_del(&c_dev);
    device_destroy(class_ptr, dev);
    class_destroy(class_ptr);
    unregister_chrdev_region(dev, 1);
    printk(KERN_INFO "Character device unregistered\n");
}

static int open_MYDEV(struct inode *inode, struct file *file)
{
    if (device_state == DEVICE_BUSY) {
        printk(KERN_INFO "Attempt to open character device failed. BUSY: %d\n", device_state);
        return -EBUSY;
    }
    device_state = DEVICE_BUSY;
    try_module_get(THIS_MODULE);
    printk(KERN_INFO "Opened character device successfully. BUSY: %d\n", device_state);
    return RET_OK;
}

static int release_MYDEV(struct inode *inode, struct file *file)
{
    device_state = DEVICE_FREE;
    module_put(THIS_MODULE);
    printk(KERN_INFO "Released character device. BUSY: %d\n", device_state);
    return RET_OK;
}

static ssize_t read_MYDEV(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
    ssize_t bytes_read = (ssize_t) length;
    if(length == 0) {
        return 0;
    }
    prng_ptr = krealloc(prng_ptr, length, GFP_KERNEL);
    if(prng_ptr == NULL) {
        return -EFAULT;
    }
    fill_rng_buffer_MYDEV(&prng_ptr, length);
    if(copy_to_user(buffer, prng_ptr, length) != 0) {
        printk(KERN_INFO "copy_to_user couldn't copy all requested bytes\n");
        return -EFAULT;
    }
    return bytes_read;
}

static ssize_t
write_MYDEV(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    size_t bytes_written = (len < STATE_BUF_LEN * sizeof(uint64_t) ) ? len : STATE_BUF_LEN * sizeof(uint64_t);
    return (ssize_t) copy_from_user(STATE_BUF, buff, bytes_written);
}

module_init(init_MYDEV)
module_exit(cleanup_MYDEV)