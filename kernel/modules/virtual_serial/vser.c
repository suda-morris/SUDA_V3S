#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>

#include <linux/ioctl.h>
#include <linux/uaccess.h>

#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"

static DEFINE_KFIFO(vsfifo, char, 32);

struct vser_dev
{
    struct cdev cdev;
    unsigned int baud;
    struct option opt;
    wait_queue_head_t rwqh;     /* 读 等待队列头 */
    wait_queue_head_t wwqh;     /* 写 等待队列头 */
    struct fasync_struct *fapp; /* fasync_struct链表头 */
};

static struct vser_dev vsdev;
static int vser_fasync(int fd, struct file *filp, int on);
static int vser_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vser_release(struct inode *inode, struct file *filp)
{
    /* 将节点从链表删除，这样进程不会再收到信号 */
    vser_fasync(-1, filp, 0);
    return 0;
}

static ssize_t vser_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    unsigned int copied = 0;
    int ret = 0;
    /* 判断fifo是否为空 */
    if (kfifo_is_empty(&vsfifo))
    {
        /* 如果是以非阻塞的方式打开 */
        if (filp->f_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }
        /* 如果是阻塞方式打开 */
        /* 进程睡眠，直到fifo不为空或者进程被信号唤醒 */
        if (wait_event_interruptible_exclusive(vsdev.rwqh, !kfifo_is_empty(&vsfifo)))
        {
            /* 被信号唤醒 */
            return -ERESTARTSYS;
        }
    }
    /* 将内核FIFO中的数据取出，复制到用户空间 */
    ret = kfifo_to_user(&vsfifo, buf, count, &copied);

    /* 如果fifo不满，唤醒等待的写进程 */
    if (!kfifo_is_full(&vsfifo))
    {
        wake_up_interruptible(&vsdev.wwqh);
        /* 发送信号SIGIO，并设置资源的可用类型是可写 */
        kill_fasync(&vsdev.fapp, SIGIO, POLL_OUT);
    }

    return ret == 0 ? copied : ret;
}

static ssize_t vser_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    unsigned int copied = 0;
    int ret = 0;
    /* 判断fifo是否满 */
    if (kfifo_is_full(&vsfifo))
    {
        if (filp->f_flags & O_NONBLOCK)
        {
            return -EAGAIN;
        }
        /* 如果是阻塞方式打开 */
        /* 进程睡眠，直到fifo不为满或者进程被信号唤醒 */
        if (wait_event_interruptible_exclusive(vsdev.wwqh, !kfifo_is_full(&vsfifo)))
        {
            /* 被信号唤醒 */
            return -ERESTARTSYS;
        }
    }
    /* 将用户空间的数据放入内核FIFO中 */
    ret = kfifo_from_user(&vsfifo, buf, count, &copied);

    /* 如果fifo不空，唤醒等待的读进程 */
    if (!kfifo_is_empty(&vsfifo))
    {
        wake_up_interruptible(&vsdev.rwqh);
        /* 发送信号SIGIO，并设置资源的可用类型是可读 */
        kill_fasync(&vsdev.fapp, SIGIO, POLL_IN);
    }
    return ret == 0 ? copied : ret;
}

static long vser_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (_IOC_TYPE(cmd) != VS_MAGIC)
    {
        return -ENOTTY; /* 参数不对 */
    }
    switch (cmd)
    {
    case VS_SET_BAUD: /* 设置波特率 */
        vsdev.baud = *(unsigned int *)arg;
        break;
    case VS_GET_BAUD:
        *(unsigned int *)arg = vsdev.baud;
        break;
    case VS_SET_FMT: /* 设置帧格式 */
        /* 返回未复制成功的字节数，该函数可能会使进程睡眠 */
        if (copy_from_user(&vsdev.opt, (struct option __user *)arg, sizeof(struct option)))
        {
            return -EFAULT;
        }
        break;
    case VS_GET_FMT:
        /* 返回未复制成功的字节数，该函数可能会使进程睡眠 */
        if (copy_to_user((struct option __user *)arg, &vsdev.opt, sizeof(struct option)))
        {
            return -EFAULT;
        }
        break;
    default:
        return -ENOTTY;
    }
    return 0;
}

static unsigned int vser_poll(struct file *filp, struct poll_table_struct *p)
{
    unsigned int mask = 0;

    /* 将系统调用中构造的等待队列节点加入到相应的等待队列中 */
    poll_wait(filp, &vsdev.rwqh, p);
    poll_wait(filp, &vsdev.wwqh, p);

    /* 根据资源的情况返回设置mask的值并返回 */
    if (!kfifo_is_empty(&vsfifo))
    {
        mask |= POLLIN | POLLRDNORM;
    }
    if (!kfifo_is_full(&vsfifo))
    {
        mask |= POLLOUT | POLLWRNORM;
    }
    return mask;
}

static int vser_fasync(int fd, struct file *filp, int on)
{
    /* 调用fasync_helper来构造struct fasync_struct节点，并加入到链表 */
    return fasync_helper(fd, filp, on, &vsdev.fapp);
}

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
    .unlocked_ioctl = vser_ioctl,
    .poll = vser_poll,
    .fasync = vser_fasync,
};

/* 模块初始化函数 */
static int __init vser_init(void)
{
    int ret;
    dev_t dev;

    dev = MKDEV(VSER_MAJOR, VSER_MINOR);
    /* 向内核注册设备号，静态方式 */
    ret = register_chrdev_region(dev, VSER_DEV_CNT, VSER_DEV_NAME);
    if (ret)
    {
        goto reg_err;
    }
    /* 初始化cdev对象，绑定ops */
    cdev_init(&vsdev.cdev, &vser_ops);
    vsdev.cdev.owner = THIS_MODULE;
    vsdev.baud = 115200;
    vsdev.opt.datab = 8;
    vsdev.opt.stopb = 1;
    vsdev.opt.parity = 0;
    /* 初始化等待队列头 */
    init_waitqueue_head(&vsdev.rwqh);
    init_waitqueue_head(&vsdev.wwqh);
    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&vsdev.cdev, dev, VSER_DEV_CNT);
    if (ret)
    {
        goto add_err;
    }
    return 0;

add_err:
    unregister_chrdev_region(dev, VSER_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit vser_exit(void)
{
    dev_t dev;

    dev = MKDEV(VSER_MAJOR, VSER_MINOR);
    cdev_del(&vsdev.cdev);
    unregister_chrdev_region(dev, VSER_DEV_CNT);
}

/* 为模块初始化函数和清理函数取别名 */
module_init(vser_init);
module_exit(vser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("virtual-serial");