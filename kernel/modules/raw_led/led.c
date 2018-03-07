#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/io.h>
#include <linux/ioport.h>

#include "led.h"

#define LED_MAJOR 255
#define LED_MINOR 0
#define LED_DEV_CNT 1
#define LED_DEV_NAME "led"

/* SFR寄存器基地址 */
#define GPIO_BASE (0x01C20800)
#define GPG_BASE (GPIO_BASE + 0x24 * 6)

struct led_dev
{
    struct cdev cdev;
    atomic_t available;
    unsigned int __iomem *pgcfg; /* SFR映射后的虚拟地址 */
    unsigned int __iomem *pgdata;
};

static struct led_dev *led;

static int led_open(struct inode *inode, struct file *filp)
{
    if (atomic_dec_and_test(&led->available))
    {
        return 0;
    }
    else
    {
        atomic_inc(&led->available);
        return -EBUSY;
    }
}

static int led_release(struct inode *inode, struct file *filp)
{
    /* 熄灭LED */
    writel(readl(led->pgdata) & ~(0x7 << 0), led->pgdata);
    atomic_inc(&led->available);
    return 0;
}

static long led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (_IOC_TYPE(cmd) != LED_MAGIC)
    {
        return -ENOTTY;
    }
    switch (cmd)
    {
    case LED_ON:
        switch (arg)
        {
        case LED_RED:
            writel(readl(led->pgdata) | (0x1 << 0), led->pgdata);
            break;
        case LED_GREEN:
            writel(readl(led->pgdata) | (0x1 << 1), led->pgdata);
            break;
        case LED_BLUE:
            writel(readl(led->pgdata) | (0x1 << 2), led->pgdata);
            break;
        default:
            return -ENOTTY;
        }
        break;
    case LED_OFF:
        switch (arg)
        {
        case LED_RED:
            writel(readl(led->pgdata) & ~(0x1 << 0), led->pgdata);
            break;
        case LED_GREEN:
            writel(readl(led->pgdata) & ~(0x1 << 1), led->pgdata);
            break;
        case LED_BLUE:
            writel(readl(led->pgdata) & ~(0x1 << 2), led->pgdata);
            break;
        default:
            return -ENOTTY;
        }
        break;
    default:
        return -ENOTTY;
    }
    return 0;
}

static struct file_operations led_ops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .unlocked_ioctl = led_ioctl,
};

/* 模块初始化函数 */
static int __init led_init(void)
{
    int ret;
    dev_t dev;

    dev = MKDEV(LED_MAJOR, LED_MINOR);
    ret = register_chrdev_region(dev, LED_DEV_CNT, LED_DEV_NAME);
    if (ret)
    {
        goto reg_err;
    }

    led = kzalloc(sizeof(struct led_dev), GFP_KERNEL);
    if (!led)
    {
        ret = -ENOMEM;
        goto mem_err;
    }

    /* 初始化cdev对象，绑定ops */
    cdev_init(&led->cdev, &led_ops);
    led->cdev.owner = THIS_MODULE;

    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&led->cdev, dev, LED_DEV_CNT);
    if (ret)
    {
        goto add_err;
    }

    atomic_set(&led->available, 1);

    /* SFR映射操作 */
    led->pgcfg = ioremap(GPG_BASE, 36);
    if (led->pgcfg)
    {
        ret = -EBUSY;
        goto map_err;
    }
    led->pgdata = led->pgcfg + 4;

    /* 初始化PG0，PG1，PG2为输出模式 */
    writel((readl(led->pgcfg) & ~(0x777 << 0)) | (0x111 << 0), led->pgcfg);
    /* 熄灭LED */
    writel(readl(led->pgdata) & ~(0x7 << 0), led->pgdata);
    return 0;

map_err:
    cdev_del(&led->cdev);
add_err:
    kfree(led);
mem_err:
    unregister_chrdev_region(dev, LED_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit led_exit(void)
{
    dev_t dev;

    dev = MKDEV(LED_MAJOR, LED_MINOR);
    /* 解除IO内存映射 */
    iounmap(led->pgcfg);

    cdev_del(&led->cdev);
    kfree(led);
    unregister_chrdev_region(dev, LED_DEV_CNT);
}

/* 为模块初始化函数和清理函数取别名 */
module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("raw-led");