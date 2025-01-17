#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/version.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"
#define SWAP(a, b) \
    do {           \
        a ^= b;    \
        b ^= a;    \
        a ^= b;    \
    } while (0)
/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100
#define MAX_BUF_SIZE 100
static dev_t fib_dev = 0;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static int major = 0, minor = 0;
static ktime_t kt;
static unsigned __int128 fib_sequence(long long n)
{
    kt = ktime_get();
    __int128 a = 0;  // F(0) = 0
    __int128 b = 1;  // F(1) = 1

    if (n == 0)
        goto end;

    unsigned int h = (sizeof(n) << 3) - __builtin_ctz(n);

    for (int j = h - 1; j >= 0; --j) {
        // n_j = floor(n / 2^j) = n >> j, k = floor(n_j / 2), (n_j = n when j =
        // 0) then a = F(k), b = F(k+1) now.
        __int128 c = a * (2 * b - a);  // F(2k) = F(k) * [ 2 * F(k+1) – F(k) ]
        __int128 d = a * a + b * b;    // F(2k+1) = F(k)^2 + F(k+1)^2

        if ((n >> j) & 1) {  // n_j is odd: k = (n_j-1)/2 => n_j = 2k + 1
            a = d;           //   F(n_j) = F(2k+1)
            b = c + d;       //   F(n_j + 1) = F(2k + 2) = F(2k) + F(2k+1)
        } else {             // n_j is even: k = n_j/2 => n_j = 2k
            a = c;           //   F(n_j) = F(2k)
            b = d;           //   F(n_j + 1) = F(2k + 1)
        }
    }
end:
    kt = ktime_sub(ktime_get(), kt);
    return a;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use\n");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    // strncpy(buf, "hello word", size);
    unsigned __int128 ret = fib_sequence(*offset);
    int cnt = 0;
    if (ret == 0) {
        buf[0] = 0 + '0';
        buf[1] = '\0';
        return 1;
    }
    while (ret > 0) {
        unsigned int a = ret % 10;
        char c = a + '0';
        ret /= 10;
        buf[cnt] = c;
        cnt++;
    }
    for (int i = 0; i < cnt / 2; i++) {
        SWAP(buf[i], buf[cnt - 1 - i]);
    }
    buf[cnt] = '\0';

    return cnt;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    /** return ktime_to_ns(kt); */
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;
    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = major = register_chrdev(major, DEV_FIBONACCI_NAME, &fib_fops);
    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev\n");
        rc = -2;
        goto failed_cdev;
    }
    fib_dev = MKDEV(major, minor);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    fib_class = class_create(DEV_FIBONACCI_NAME);
#else
    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);
#endif
    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class\n");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device\n");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
failed_cdev:
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    unregister_chrdev(major, DEV_FIBONACCI_NAME);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
