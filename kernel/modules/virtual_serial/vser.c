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

#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"
#define VSRE_FIFO_SIZE 32
static DEFINE_KFIFO(vsfifo, char, VSRE_FIFO_SIZE);

struct vser_dev
{
    struct cdev cdev;
    unsigned int baud;
    struct option opt;
    wait_queue_head_t rwqh;     /* 读 等待队列头 */
    struct fasync_struct *fapp; /* fasync_struct链表头 */
    struct hrtimer timer;       /* 高分辨率定时器对象 */
    atomic_t available;         /* 当前串口是否可用 */
};

static struct vser_dev vsdev;
static int vser_fasync(int fd, struct file *filp, int on);
static int vser_open(struct inode *inode, struct file *filp)
{
    if (atomic_dec_and_test(&vsdev.available))
    {
        /* 首次被打开 */
        return 0;
    }
    else
    {
        /* 竞争失败的进程应该要将available的值加回来，否则以后串口将永远无法打开 */
        atomic_inc(&vsdev.available);
        return -EBUSY;
    }
}

static int vser_release(struct inode *inode, struct file *filp)
{
    /* 将节点从链表删除，这样进程不会再收到信号 */
    vser_fasync(-1, filp, 0);
    /* 使用完串口之后要把available加回来，使串口又可用 */
    atomic_inc(&vsdev.available);
    return 0;
}

static ssize_t vser_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    int len;
    int ret;
    char rbuf[VSRE_FIFO_SIZE];
    /* 修正读取数据的大小 */
    len = count > sizeof(rbuf) ? sizeof(rbuf) : count;
    /* 使用等待队列中自带的自旋锁进行加锁操作，保护共享资源fifo */
    /* 在持有锁期间，不能调度调度器 */
    spin_lock(&vsdev.rwqh.lock);
    /* 判断fifo是否为空 */
    if (kfifo_is_empty(&vsfifo))
    {
        /* 如果是以非阻塞的方式打开 */
        if (filp->f_flags & O_NONBLOCK)
        {
            spin_unlock(&vsdev.rwqh.lock);
            return -EAGAIN;
        }
        /* 如果是阻塞方式打开 */
        /* 进程睡眠，直到fifo不为空或者进程被信号唤醒 */
        /* wait_event_interruptible_locked会在进程休眠前自动释放自旋锁，醒来后又将重新获得自旋锁 */
        if (wait_event_interruptible_locked(vsdev.rwqh, !kfifo_is_empty(&vsfifo)))
        {
            spin_unlock(&vsdev.rwqh.lock);
            /* 被信号唤醒 */
            return -ERESTARTSYS;
        }
    }
    /* 将内核FIFO中的数据取出，复制到临时缓冲区 */
    len = kfifo_out(&vsfifo, rbuf, len);
    /* 释放自旋锁 */
    spin_unlock(&vsdev.rwqh.lock);

    ret = copy_to_user(buf, rbuf, len);

    return len - ret;
}

static ssize_t vser_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    int ret;
    int len;
    char *tbuf[VSRE_FIFO_SIZE];
    len = count > sizeof(tbuf) ? sizeof(tbuf) : count;
    ret = copy_from_user(tbuf, buf, len);
    return len - ret;
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

    spin_lock(&vsdev.rwqh.lock);
    /* 根据资源的情况返回设置mask的值并返回 */
    if (!kfifo_is_empty(&vsfifo))
    {
        mask |= POLLIN | POLLRDNORM;
    }
    spin_unlock(&vsdev.rwqh.lock);
    return mask;
}

static int vser_fasync(int fd, struct file *filp, int on)
{
    /* 调用fasync_helper来构造struct fasync_struct节点，并加入到链表 */
    return fasync_helper(fd, filp, on, &vsdev.fapp);
}

static enum hrtimer_restart vser_timer(struct hrtimer *timer)
{
    unsigned char data;

    get_random_bytes(&data, sizeof(data));
    data %= 26;
    data += 'A';
    spin_lock(&vsdev.rwqh.lock);
    if (!kfifo_is_full(&vsfifo))
    {
        if (!kfifo_in(&vsfifo, &data, sizeof(data)))
        {
            printk(KERN_ERR "vser: kfifo_in failure\n");
        }
    }
    if (!kfifo_is_empty(&vsfifo))
    {
        spin_unlock(&vsdev.rwqh.lock);
        wake_up_interruptible(&vsdev.rwqh);
        kill_fasync(&vsdev.fapp, SIGIO, POLL_IN);
    }
    else
    {
        spin_unlock(&vsdev.rwqh.lock);
    }

    /* 重新设置定时值 */
    hrtimer_forward_now(timer, ktime_set(1, 1000));
    /* HRTIMER_RESTART表示要重新启动定时器 */
    return HRTIMER_RESTART;
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
    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&vsdev.cdev, dev, VSER_DEV_CNT);
    if (ret)
    {
        goto add_err;
    }
    /* 初始化定时器 */
    /* CLOCK_MONOTONIC时钟类型表示自系统开机以来的单调递增时间,HRTIMER_MODE_REL表示相对时间模式 */
    hrtimer_init(&vsdev.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    vsdev.timer.function = vser_timer;
    /* 启动定时器 */
    hrtimer_start(&vsdev.timer, ktime_set(1, 1000), HRTIMER_MODE_REL);
    /* 将原子变量赋值为1 */
    atomic_set(&vsdev.available, 1);
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

    /* 关闭定时器 */
    hrtimer_cancel(&vsdev.timer);
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