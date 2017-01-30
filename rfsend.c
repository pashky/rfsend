#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt "\n"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>


#define  DEVICE_NAME "rfsend"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pashky");
MODULE_DESCRIPTION("433mHz on GPIO sender");
MODULE_VERSION("0.1");

#define MAX_PACKET 4096

#ifndef MAX_UDELAY_MS
#define MAX_UDELAY_US 5000
#else
#define MAX_UDELAY_US (MAX_UDELAY_MS*1000)
#endif

static int rfs_major;
static struct cdev rfs_device;
static struct class *rfs_class = NULL;
static int tx_pin = 4;
static int is_open = 0;
static char packet_buf[MAX_PACKET] = {0};

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

module_param(tx_pin, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(tx_pin, "transmitter pin");

static struct file_operations fops =
{
    .open = dev_open,
    .write = dev_write,
    .release = dev_release,
};

struct rfs_packet {
    int length[26];
    char *sequence;
    int start_from;
};


static int __init rfs_init(void) {
    pr_devel("initializing");

    dev_t dev = 0;
    int result = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (result < 0){
        pr_warn("failed to register a major number");
        return result;
    }

    if ((rfs_class = class_create(THIS_MODULE, DEVICE_NAME)) == NULL)
    {
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    if (device_create(rfs_class, NULL, dev, NULL, DEVICE_NAME) == NULL)
    {
        class_destroy(rfs_class);
        unregister_chrdev_region(dev, 1);
        return -1;
    }
    
    rfs_major = MAJOR(dev);
    pr_devel("registered correctly with major number %d", rfs_major);

    cdev_init(&rfs_device, &fops);
    result = cdev_add(&rfs_device, dev, 1);
    if (result) {
        pr_warn("failed to add %d", result);
        return -EFAULT;
    }

    result = gpio_request_one(tx_pin, GPIOF_OUT_INIT_LOW, DEVICE_NAME);
    if (result) {
        pr_warn("could not request pin %d, error: %d", tx_pin, result);
        return -EIO;
    }

    pr_info("433mHz transmitter on pin %d", tx_pin);

    return 0;
}

static void __exit rfs_exit(void) {
    gpio_free(tx_pin);
    
    cdev_del(&rfs_device);
    device_destroy(rfs_class, MKDEV(rfs_major, 0));
    class_destroy(rfs_class);
    unregister_chrdev_region(MKDEV(rfs_major, 0), 1);
}

static void safe_udelay(unsigned long usecs)
{
	while (usecs > MAX_UDELAY_US) {
		udelay(MAX_UDELAY_US);
		usecs -= MAX_UDELAY_US;
	}
	udelay(usecs);
}
int rfs_parse(char *buf, struct rfs_packet *packet) {
    memset(packet->length, 0, sizeof(packet->length));
    for(char *p = buf; *p; ) {
        if (*p >= 'a' && *p <= 'z') {
            int index = *p++ - 'a';
            if (*p == '\0') {
                return -EINVAL;
            }
            int length = 0;
            for (; *p >= '0' && *p <= '9'; ++p) {
                length = length * 10 + (*p - '0');
            }
            packet->length[index] = length;
            
        } else if (*p == '^' || *p == '_') {
            packet->start_from = *p == '^' ? 1 : 0;
            char *start = ++p;
            for (; *p >= 'a' && *p <= 'z'; ++p);
            if (*p != '\0' && *p != '\r' && *p != '\n') {
                return -EINVAL;
            }
            *p = '\0';
            packet->sequence = start;
            return 0;
            
        } else {
            return -EINVAL;
        }
    }
    return -EINVAL;
}

int rfs_send(const struct rfs_packet *pkt) {
    gpio_set_value(tx_pin, pkt->start_from);
    usleep_range(50000, 100000);
    gpio_set_value(tx_pin, pkt->start_from ^ 1);
    usleep_range(50000, 100000);

    DEFINE_SPINLOCK(lock);
    unsigned long flags;

    spin_lock_irqsave(&lock, flags);

    int level = pkt->start_from;
    for(char *seq = pkt->sequence; *seq; ++seq) {
        int delay_us = pkt->length[*seq - 'a'];
        gpio_set_value(tx_pin, level);
        safe_udelay(delay_us);
        level ^= 1;
    }
    gpio_set_value(tx_pin, level);

    spin_unlock_irqrestore(&lock, flags);

    return 0;
}

static int dev_open(struct inode *inodep, struct file *filep) {
    if (is_open) {
        return -EBUSY;
    }
    
    try_module_get(THIS_MODULE);
    ++is_open;
    pr_devel("device has been opened");
    return 0;
}


static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    int result = strncpy_from_user(packet_buf, buffer, len < MAX_PACKET ? len : MAX_PACKET);
    if (result < 0) {
		return -EIO;
    }

    pr_devel("got packet %s", packet_buf);

    struct rfs_packet packet;
    result = rfs_parse(packet_buf, &packet);
    if (result < 0) {
        pr_warn("can't parse packet");
        return -EIO;
    }

    pr_devel("parsed packet");
    
    rfs_send(&packet);
    return len;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    --is_open;
    module_put(THIS_MODULE);
    pr_devel("device has been closed");
    return 0;
}

module_init(rfs_init);
module_exit(rfs_exit);
