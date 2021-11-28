#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>
#include <linux/math.h>

#include "data.h"

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
static uint32_t STATE_BUF[STATE_BUF_LEN];

#define MODE_UNIVERSAL 0
#define MODE_GAUSS 1
#define MODE_BETA 2

static int generator_mode = MODE_UNIVERSAL;

static long gauss_stddev = 1;
static long gauss_mean = 0;

static long beta_alpha = 1;
static long beta_beta = 1;
#define URN_ITERATIONS 200

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

/* xoshiro128** impl */

static uint32_t xos_rol32(uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
}

uint64_t xoshiro128ss(uint32_t* state_ptr) {
    uint32_t *s = state_ptr;
    uint32_t const result = xos_rol32(s[1] * 5, 7) * 9;
    uint32_t const t = s[1] << 9;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];

    s[2] ^= t;
    s[3] = xos_rol32(s[3], 11);

    return result;
}

/* --       -- */

static void get_gaussian(const uint16_t* uni_source, int32_t ** result, size_t length) {
    int64_t adjustment = 500000000000 + 2360000000 * gauss_stddev + 12000000 * gauss_stddev * gauss_stddev;
    int32_t * helper_ptr = *result;
    size_t i = 0;
    size_t to = length / (sizeof(uint32_t));
    for(; i < to; i+=2) {
        uint16_t U1 = uni_source[i] % 512;
        uint16_t U2 = uni_source[i+1] % 512;
        int64_t Z1_lhs = gaussian_lookup_table_sqrt[U1];
        int64_t Z2_lhs = gaussian_lookup_table_sqrt[U1];
        Z1_lhs *= gaussian_lookup_table_sin[(U2)];
        Z2_lhs *= gaussian_lookup_table_cos[(U2)];
        Z1_lhs *= (int64_t)gauss_stddev;
        Z2_lhs *= (int64_t) gauss_stddev;
        Z1_lhs += (Z1_lhs >= 0) ? adjustment : -adjustment;
        Z2_lhs += (Z2_lhs >= 0) ? adjustment : -adjustment;
        Z1_lhs /= 1000000000000;
        Z2_lhs /= 1000000000000;
        Z1_lhs += (int32_t) gauss_mean;
        Z2_lhs += (int32_t) gauss_mean;

        helper_ptr[i] = (int32_t)Z1_lhs;
        helper_ptr[i+1] = (int32_t)Z2_lhs;
    }
}

static void get_beta(uint32_t ** result, size_t length) {
    uint32_t * helper_ptr = *result;
    size_t i = 0;
    for(; i < length/sizeof(uint32_t); ++i) {
        uint32_t alpha_count = beta_alpha;
        uint32_t beta_count = beta_beta;
        uint32_t sum = alpha_count + beta_count;
        uint32_t drawn = 0;
        size_t j = 0;
        for(; j < URN_ITERATIONS; ++j) {
            uint32_t universal_result = xoshiro128ss(STATE_BUF) % sum;
            if(universal_result < alpha_count) {
                alpha_count++;
                sum++;
            } else {
                beta_count++;
                sum++;
                drawn++;
            }

        }
        helper_ptr[i] = drawn;
    }
}

static void seed_MYDEV(void) {
    /* Incredible initial seed generator */
    uint32_t i;
    for (i = 0; i < STATE_BUF_LEN; ++i)
        STATE_BUF[i] = ktime_get_ns();
}

static void fill_rng_buffer_MYDEV(uint32_t ** buf_ptr, size_t length) {
    size_t div = (size_t) length / sizeof(uint32_t);
    size_t rest = length % sizeof(uint32_t);
    uint32_t* helper_ptr = *buf_ptr;
    size_t i;
    for(i=0; i < div; ++i) {
        helper_ptr[i] = xoshiro128ss(STATE_BUF);
    }
    if(rest != 0) {
        uint32_t rn = xoshiro128ss(STATE_BUF);
        memcpy(helper_ptr+div, &rn, rest);
    }
    printk(KERN_INFO "Filled!\n");
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

static ssize_t generate_universal(char* buffer, size_t length) {
    uint32_t * prng_ptr = kmalloc(length, GFP_KERNEL);
    if(prng_ptr == NULL) {
        kfree(prng_ptr);
        return -EFAULT;
    }
    fill_rng_buffer_MYDEV(&prng_ptr, length);
    if(copy_to_user(buffer, prng_ptr, length) != 0) {
        kfree(prng_ptr);
        return -EFAULT;
    }
    return (ssize_t) length;
}

static ssize_t generate_gauss(char* buffer, size_t length) {
    uint16_t* universal_ptr = kmalloc(2*(length / sizeof(uint32_t)), GFP_KERNEL);
    int32_t* gaussian_ptr = kmalloc(length, GFP_KERNEL);
    fill_rng_buffer_MYDEV((uint32_t**)&universal_ptr, 2*(length/sizeof(uint32_t)));
    get_gaussian(universal_ptr, &gaussian_ptr, length);
    if(copy_to_user(buffer, gaussian_ptr, length) != 0) {
        kfree(gaussian_ptr);
        kfree(universal_ptr);
        return -EFAULT;
    }
    kfree(universal_ptr);
    kfree(gaussian_ptr);
    return (ssize_t) length;
}

static ssize_t generate_beta(char* buffer, size_t length) {
    uint32_t* beta_ptr = kmalloc(length, GFP_KERNEL);
    printk(KERN_INFO "Allocated beta_ptr\n");
    get_beta(&beta_ptr, length);
    if(copy_to_user(buffer, beta_ptr, length) != 0) {
        kfree(beta_ptr);
        return -EFAULT;
    }
    kfree(beta_ptr);
    return (ssize_t) length;
}

static ssize_t read_MYDEV(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
    if(length == 0) {
        return 0;
    }

    switch(generator_mode) {
        case(MODE_UNIVERSAL):
            return generate_universal(buffer, length);
        case(MODE_GAUSS):
            return generate_gauss(buffer, length);
        case(MODE_BETA):
            return generate_beta(buffer, length);
        default:
            return -EFAULT;
    }

}

static ssize_t
write_MYDEV(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    char * read_data = kmalloc(len, GFP_KERNEL);
    if(copy_from_user(read_data, buff, len))
        return -EFAULT;
    if(strncmp("seed:", read_data, 5) == 0) {
        size_t seed_len = (len < STATE_BUF_LEN  * sizeof(uint32_t) + 5 ) ? len - 5: STATE_BUF_LEN * sizeof(uint32_t);
        memcpy(STATE_BUF, read_data+5, seed_len);
    } else if(strncmp("gauss:", read_data, 6) == 0) {
        size_t i = 6;
        size_t j;
        char * dev_data;
        char * mean_data;
        generator_mode = MODE_GAUSS;
        while(read_data[i] == ' ') i++;
        j = i;
        while(read_data[j] != ' ') j++;
        dev_data = kmalloc(j - i + 1, GFP_KERNEL);
        memcpy(dev_data, read_data + i, j - i);
        dev_data[j-i] = '\0';
        if(kstrtol(dev_data, 0, &gauss_stddev) != 0 ) {
            kfree(read_data);
            kfree(dev_data);
            return -EFAULT;
        }

        i = j + 1;
        while(read_data[i] == ' ') i++;
        j = i;
        while(read_data[j] != ' ' && read_data[j] != '\0' && read_data[j] != '\n') j++;
        mean_data = kmalloc(j - i + 1, GFP_KERNEL);
        mean_data[j-i] = '\0';
        memcpy(mean_data, read_data + i, j - i);
        if(kstrtol(mean_data, 0, &gauss_mean) != 0 ) {
            kfree(read_data);
            kfree(dev_data);
            kfree(mean_data);
            return -EFAULT;
        }

        kfree(dev_data);
        kfree(mean_data);

    } else if(strncmp("beta:", read_data, 5) == 0) {
        size_t i = 5;
        size_t j;
        char * alpha_data;
        char * beta_data;
        generator_mode = MODE_BETA;
        while(read_data[i] == ' ') i++;
        j = i;
        while(read_data[j] != ' ') j++;
        alpha_data = kmalloc(j - i + 1, GFP_KERNEL);
        memcpy(alpha_data, read_data + i, j - i);
        alpha_data[j-i] = '\0';
        if(kstrtol(alpha_data, 0, &beta_alpha) != 0 ) {
            kfree(read_data);
            kfree(alpha_data);
            return -EFAULT;
        }
        printk(KERN_INFO "beta alpha: %ld", beta_alpha);

        i = j + 1;
        while(read_data[i] == ' ') i++;
        j = i;
        while(read_data[j] != ' ' && read_data[j] != '\0' && read_data[j] != '\n') j++;
        beta_data = kmalloc(j - i + 1, GFP_KERNEL);
        beta_data[j-i] = '\0';
        memcpy(beta_data, read_data + i, j - i);
        if(kstrtol(beta_data, 0, &beta_beta) != 0 ) {
            kfree(read_data);
            kfree(alpha_data);
            kfree(beta_data);
            return -EFAULT;
        }

        printk(KERN_INFO "beta beta: %ld", beta_beta);

        kfree(alpha_data);
        kfree(beta_data);
    } else if(strncmp("universal", read_data, 9) == 0) {
        generator_mode = MODE_UNIVERSAL;
    } else {
        printk(KERN_INFO "Unknown write\n");
    }
    kfree(read_data);
    return (ssize_t) len;
}

module_init(init_MYDEV)
module_exit(cleanup_MYDEV)
