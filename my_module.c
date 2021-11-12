#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/ucontext.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>
#include <asm/fpu/api.h>
#include <linux/math.h>

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


int32_t gaussian_lookup_table_sqrt[256] = {
        0,
        3329,
        3113,
        2980,
        2882,
        2804,
        2738,
        2681,
        2631,
        2586,
        2545,
        2507,
        2472,
        2439,
        2409,
        2380,
        2353,
        2327,
        2302,
        2278,
        2256,
        2234,
        2213,
        2193,
        2174,
        2155,
        2136,
        2119,
        2101,
        2085,
        2068,
        2052,
        2037,
        2022,
        2007,
        1992,
        1978,
        1964,
        1951,
        1937,
        1924,
        1911,
        1899,
        1886,
        1874,
        1862,
        1850,
        1839,
        1827,
        1816,
        1805,
        1794,
        1783,
        1772,
        1761,
        1751,
        1741,
        1731,
        1720,
        1710,
        1701,
        1691,
        1681,
        1672,
        1662,
        1653,
        1644,
        1634,
        1625,
        1616,
        1607,
        1599,
        1590,
        1581,
        1573,
        1564,
        1555,
        1547,
        1539,
        1530,
        1522,
        1514,
        1506,
        1498,
        1490,
        1482,
        1474,
        1466,
        1458,
        1450,
        1443,
        1435,
        1427,
        1420,
        1412,
        1405,
        1397,
        1390,
        1382,
        1375,
        1368,
        1360,
        1353,
        1346,
        1339,
        1332,
        1325,
        1317,
        1310,
        1303,
        1296,
        1289,
        1282,
        1275,
        1268,
        1262,
        1255,
        1248,
        1241,
        1234,
        1227,
        1221,
        1214,
        1207,
        1200,
        1194,
        1187,
        1180,
        1174,
        1167,
        1160,
        1154,
        1147,
        1140,
        1134,
        1127,
        1121,
        1114,
        1108,
        1101,
        1095,
        1088,
        1082,
        1075,
        1069,
        1062,
        1056,
        1049,
        1043,
        1036,
        1030,
        1023,
        1017,
        1010,
        1004,
        997,
        991,
        984,
        978,
        971,
        965,
        959,
        952,
        946,
        939,
        933,
        926,
        920,
        913,
        907,
        900,
        893,
        887,
        880,
        874,
        867,
        861,
        854,
        847,
        841,
        834,
        827,
        821,
        814,
        807,
        801,
        794,
        787,
        780,
        773,
        767,
        760,
        753,
        746,
        739,
        732,
        725,
        718,
        711,
        704,
        697,
        689,
        682,
        675,
        668,
        660,
        653,
        645,
        638,
        630,
        623,
        615,
        607,
        599,
        592,
        584,
        576,
        568,
        559,
        551,
        543,
        534,
        526,
        517,
        509,
        500,
        491,
        482,
        473,
        463,
        454,
        444,
        434,
        424,
        414,
        404,
        393,
        382,
        371,
        359,
        348,
        336,
        323,
        310,
        296,
        282,
        268,
        252,
        235,
        218,
        199,
        177,
        153,
        125,
        88,
        0
};

int32_t gaussian_lookup_table_sin[256] = {
        0,
        24,
        49,
        73,
        98,
        122,
        147,
        171,
        195,
        219,
        243,
        267,
        291,
        314,
        338,
        361,
        384,
        406,
        429,
        451,
        473,
        494,
        515,
        536,
        557,
        577,
        597,
        617,
        636,
        655,
        673,
        691,
        709,
        726,
        743,
        759,
        775,
        790,
        805,
        819,
        833,
        846,
        859,
        872,
        883,
        895,
        905,
        916,
        925,
        934,
        943,
        951,
        958,
        965,
        971,
        976,
        981,
        986,
        989,
        993,
        995,
        997,
        999,
        999,
        999,
        999,
        998,
        996,
        994,
        991,
        988,
        984,
        979,
        974,
        968,
        961,
        954,
        947,
        938,
        930,
        920,
        911,
        900,
        889,
        878,
        866,
        853,
        840,
        826,
        812,
        798,
        782,
        767,
        751,
        734,
        717,
        700,
        682,
        664,
        645,
        626,
        607,
        587,
        567,
        547,
        526,
        505,
        483,
        462,
        440,
        417,
        395,
        372,
        349,
        326,
        303,
        279,
        255,
        231,
        207,
        183,
        159,
        135,
        110,
        86,
        61,
        36,
        12,
        -12,
        -36,
        -61,
        -86,
        -110,
        -135,
        -159,
        -183,
        -207,
        -231,
        -255,
        -279,
        -303,
        -326,
        -349,
        -372,
        -395,
        -417,
        -440,
        -462,
        -483,
        -505,
        -526,
        -547,
        -567,
        -587,
        -607,
        -626,
        -645,
        -664,
        -682,
        -700,
        -717,
        -734,
        -751,
        -767,
        -782,
        -798,
        -812,
        -826,
        -840,
        -853,
        -866,
        -878,
        -889,
        -900,
        -911,
        -920,
        -930,
        -938,
        -947,
        -954,
        -961,
        -968,
        -974,
        -979,
        -984,
        -988,
        -991,
        -994,
        -996,
        -998,
        -999,
        -999,
        -999,
        -999,
        -997,
        -995,
        -993,
        -989,
        -986,
        -981,
        -976,
        -971,
        -965,
        -958,
        -951,
        -943,
        -934,
        -925,
        -916,
        -905,
        -895,
        -883,
        -872,
        -859,
        -846,
        -833,
        -819,
        -805,
        -790,
        -775,
        -759,
        -743,
        -726,
        -709,
        -691,
        -673,
        -655,
        -636,
        -617,
        -597,
        -577,
        -557,
        -536,
        -515,
        -494,
        -473,
        -451,
        -429,
        -406,
        -384,
        -361,
        -338,
        -314,
        -291,
        -267,
        -243,
        -219,
        -195,
        -171,
        -147,
        -122,
        -98,
        -73,
        -49,
        -24,
        0
};

int32_t gaussian_lookup_table_cos[256] = {
        0,
        999,
        998,
        997,
        995,
        992,
        989,
        985,
        980,
        975,
        969,
        963,
        956,
        949,
        941,
        932,
        923,
        913,
        903,
        892,
        881,
        869,
        856,
        843,
        830,
        816,
        801,
        786,
        771,
        755,
        739,
        722,
        704,
        687,
        669,
        650,
        631,
        612,
        592,
        572,
        552,
        531,
        510,
        489,
        467,
        445,
        423,
        401,
        378,
        355,
        332,
        309,
        285,
        261,
        237,
        213,
        189,
        165,
        141,
        116,
        92,
        67,
        43,
        18,
        -6,
        -30,
        -55,
        -79,
        -104,
        -128,
        -153,
        -177,
        -201,
        -225,
        -249,
        -273,
        -297,
        -320,
        -343,
        -366,
        -389,
        -412,
        -434,
        -456,
        -478,
        -499,
        -521,
        -542,
        -562,
        -582,
        -602,
        -622,
        -641,
        -659,
        -678,
        -696,
        -713,
        -730,
        -747,
        -763,
        -779,
        -794,
        -809,
        -823,
        -836,
        -850,
        -862,
        -875,
        -886,
        -897,
        -908,
        -918,
        -927,
        -936,
        -945,
        -952,
        -960,
        -966,
        -972,
        -978,
        -982,
        -987,
        -990,
        -993,
        -996,
        -998,
        -999,
        -999,
        -999,
        -999,
        -998,
        -996,
        -993,
        -990,
        -987,
        -982,
        -978,
        -972,
        -966,
        -960,
        -952,
        -945,
        -936,
        -927,
        -918,
        -908,
        -897,
        -886,
        -875,
        -862,
        -850,
        -836,
        -823,
        -809,
        -794,
        -779,
        -763,
        -747,
        -730,
        -713,
        -696,
        -678,
        -659,
        -641,
        -622,
        -602,
        -582,
        -562,
        -542,
        -521,
        -500,
        -478,
        -456,
        -434,
        -412,
        -389,
        -366,
        -343,
        -320,
        -297,
        -273,
        -249,
        -225,
        -201,
        -177,
        -153,
        -128,
        -104,
        -79,
        -55,
        -30,
        -6,
        18,
        43,
        67,
        92,
        116,
        141,
        165,
        189,
        213,
        237,
        261,
        285,
        309,
        332,
        355,
        378,
        401,
        423,
        445,
        467,
        489,
        510,
        531,
        552,
        572,
        592,
        612,
        631,
        650,
        669,
        687,
        704,
        722,
        739,
        755,
        771,
        786,
        801,
        816,
        830,
        843,
        856,
        869,
        881,
        892,
        903,
        913,
        923,
        932,
        941,
        949,
        956,
        963,
        969,
        975,
        980,
        985,
        989,
        992,
        995,
        997,
        998,
        999,
        1000
};

#define DEVICE_DEV_NAME "__prng_device" /* as it shows up in /dev/ etc. */
#define DEVICE_CLASS_NAME DEVICE_DEV_NAME
#define DEVICE_NAME_FORMAT DEVICE_DEV_NAME

#define RET_OK 0
#define RET_ERR (-1)
#define DEVICE_BUSY 1
#define DEVICE_FREE 0
#define STATE_BUF_LEN 4

static int device_state = DEVICE_FREE;
static uint32_t *prng_ptr;
static uint32_t STATE_BUF[STATE_BUF_LEN];

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

//scaled = mean + stdDev * zScore


static void get_gaussian(const uint8_t* uni_source, int32_t ** result, size_t length) {
    int32_t * helper_ptr = *result;
    size_t i = 0;
    //printk(KERN_INFO "Buffer length: %zu \n", length);
//    size_t j = 0;
//    for(j = 0; j < length; j++) {
//        printk(KERN_INFO "Buffer Value: %u \n", helper_ptr[j]);
//    }
    size_t to = length / (2 * sizeof(uint32_t));
    printk(KERN_INFO "Iterating from 0 to :, %zu :: length %zu, size %lu\n", to, length, sizeof(uint32_t));
    for(; i < to; ++i) {
        printk(KERN_INFO "Iterating in for, i: %zu\n", i);
        // For length == 16, which would be 4 int32_t:
        // for(i=0; i < 2; i+=2) <= 2 iterations
        // 0    0 1
        // 1    2 3
        // 2    4 5
        // 3    6 7
        // 4    8 9
        uint8_t U1 = uni_source[2*i] % 255;         //
        uint8_t U2 = uni_source[(2*i)+1] % 255;       // 1, 3
        printk(KERN_INFO "Value from buffer: %u %u\n", U1, U2);

        int32_t Z1_lhs = gaussian_lookup_table_sqrt[U1];
        int32_t Z2_lhs = gaussian_lookup_table_sqrt[U1];
        printk(KERN_INFO "Sqrt for that: %d %d\n", Z1_lhs, Z2_lhs);

        Z1_lhs *= gaussian_lookup_table_sin[(U2)];
        printk(KERN_INFO "Sin: %d\n", gaussian_lookup_table_sin[(U2)]);
        Z2_lhs *= gaussian_lookup_table_cos[(U2)];
        printk(KERN_INFO "Sin: %d\n", gaussian_lookup_table_cos[(U2)]);
        printk(KERN_INFO "SinCos for that: %d %d\n", Z1_lhs, Z2_lhs);
        Z1_lhs = Z1_lhs / 100000;
        Z2_lhs = Z1_lhs / 100000;
        //if(Z1_lhs < 0) Z1_lhs *= -1;
        //if(Z2_lhs < 0) Z2_lhs *= -1;
        printk(KERN_INFO "Final: %d %d\n", Z1_lhs, Z2_lhs);
        helper_ptr[i] = Z1_lhs;
        helper_ptr[i+1] = Z2_lhs;
    }
}

static void seed_MYDEV(void) {
    /* Incredible seed generator */
    uint32_t i;
    for (i = 0; i < STATE_BUF_LEN; ++i)
        STATE_BUF[i] = ktime_get_ns();
}

static void fill_rng_buffer_MYDEV(uint32_t ** buf_ptr, size_t length) {
    printk(KERN_INFO "Entering fill!\n");
    size_t div = (size_t) length / sizeof(uint32_t);
    size_t rest = length % sizeof(uint32_t);
    uint32_t** ptr = (uint32_t**) buf_ptr;
    uint32_t* helper_ptr = *ptr;
    size_t i = 0;
    for(i=0; i < div; ++i) {
        helper_ptr[i] = xoshiro128ss(STATE_BUF);
    }
    if(rest != 0) {
        uint32_t rn = xoshiro128ss(STATE_BUF);
        memcpy(&rn, helper_ptr+div, rest);
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

     //Universal distribution case

//    prng_ptr = krealloc(prng_ptr, length, GFP_KERNEL);
//    if(prng_ptr == NULL) {
//        return -EFAULT;
//    }
//    fill_rng_buffer_MYDEV(&prng_ptr, length);
//    if(copy_to_user(buffer, prng_ptr, length) != 0) {
//        printk(KERN_INFO "copy_to_user couldn't copy all requested bytes\n");
//        return -EFAULT;
//    }
//    printk(KERN_INFO "Returning!\n");
//    return bytes_read;

   //  -------

    printk(KERN_INFO "Requesting %zu / %lu =  %lu universal bytes\n",length, sizeof(uint32_t), length / sizeof(uint32_t));
    // For length==16, which would be 4 int32_t, allocating 4 universal bytes
    uint8_t* universal_ptr = krealloc(universal_ptr, length / sizeof(uint32_t), GFP_KERNEL);
    printk(KERN_INFO "Requesting %lu gaussian bytes\n", length);
    // For length==16, which is 4 int32_t, allocating 16 bytes
    int32_t* gaussian_ptr = krealloc(gaussian_ptr, length, GFP_KERNEL);
    fill_rng_buffer_MYDEV((uint32_t**)&universal_ptr, length);
    get_gaussian(universal_ptr, &gaussian_ptr, length);
    printk(KERN_INFO "Results %d %d\n", gaussian_ptr[0], gaussian_ptr[1]);
    //int32_t tst[8] = {1,2,3,4,5,6,7,8};
    if(copy_to_user(buffer, gaussian_ptr, length) != 0) {
        printk(KERN_INFO "copy_to_user couldn't copy all requested bytes\n");
        return -EFAULT;
    }
    printk(KERN_INFO "Returning!\n");

    return bytes_read;
}

static ssize_t
write_MYDEV(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    size_t bytes_written = (len < STATE_BUF_LEN * sizeof(uint64_t) ) ? len : STATE_BUF_LEN * sizeof(uint64_t);
    if(copy_from_user(STATE_BUF, buff, bytes_written) != bytes_written)
        return -EFAULT;
    return (ssize_t) bytes_written;
}

module_init(init_MYDEV)
module_exit(cleanup_MYDEV)
