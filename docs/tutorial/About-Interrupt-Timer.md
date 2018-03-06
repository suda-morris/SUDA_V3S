# About Interrupt && Timer
### 驱动中的中断处理

> ARM处理器的中断控制器被称为GIC，相比于一般的中断控制器而言，其最主要的特点在于可以将一个特定的中断分发给一个特定的ARM核。

* 共享中断

> 由于中断控制器的管脚有限，所以在某些体系结构上有两个或者两个以上的设备被接到了同一根中断线上，这样这两个设备中的任何一个设备产生了中断，都会触发中断，这就是共享中断。Linux内核不能判断新产生的这个中断究竟属于哪一个设备，于是将共用一根中断线的中断处理函数通过不同的struct irqaction结构包装起来，然后用链表连接起来。内核得到IRQ号后，就遍历该链表上的每一个struct irqaction对象，然后调用其handler成员所指向的中断处理函数。而在中断处理函数中，驱动开发者应该判断该中断是否是自己所管理的设备产生的，如果是则进行相应的中断处理，如果不是则直接返回。

* 注意事项

> 在整个中断处理过程中，中断是被禁止的，如果中断处理函数执行时间过长，那么其他的中断将会被挂起。必须要记住的一条准则是：**在中断处理函数中一定不能调用调度器，即一定不能调用可能会引起进程切换的函数**，比如kfifo_to_user,kfifo_from_user,copy_to_user,copy_from_user,wait_event_xxx。另外，在中断处理函数中如果调用disable_irq可能会死锁。

*向内核注册一个中断处理函数的函数原型：*

``int request_irq(unsigned int irq,irq_handler_t handler,unsigned long flags,const char* name,void* dev);``

* irq：设备上所用中断的IRQ号，这个号不是硬件手册上查到的号，而是内核中的IRQ号，这个号决定了构造的struct irqaction对象被插入到哪个链表。可以通过cat /proc/interrupts 来查看具体可用的IRQ号码
* handler：指向中断处理函数的指针**typedef irqreturn_t (\*irq_handler_t)(int,void*);**
  * 中断处理函数的第一个参数是IRQ号
  * 第二个参数是对应的设备ID，也是struct irqaction结构中的dev_id成员
  * 中断处理函数的返回值是一个枚举类型的irqreturn_t，以下数值可供选择:
    * IRQ_NONE：不是驱动所管理的设备产生的中断
    * IRQ_HANDLED：中断被正常处理
    * IRQ_WAKE_THREAD：需要唤醒一个内核线程
* flags：常用的标志如下
  * IRQF_TRIGGER_RISING：上升沿触发
  * IRQF_TRIGGER_FALLING：下降沿触发
  * IRQF_TRIGGER_HIGH：高电平触发
  * IRQF_TRIGGER_LOW：低电平触发
  * IRQF_DISABLED：中断函数执行期间禁止中断
  * IRQF_SHARED：共享中断必须设置的标志
  * IRQF_TIMER：定时器专用中断标志
* name：该中断在/proc中的名字，用于初始化struct irqaction对象
* dev：区别共享中断中不同设备所对应的struct irqaction对象，共享中断必须传递一个非NULL的实参，非共享中断可以传NULL。中断发生后，调用中断处理函数时，内核也会将该参数传给中断处理函数

*注销一个中断处理函数的函数原型如下：*

``void free_irq(unsigned int,void*);``

* 第一个参数是IRQ号，第二个参数是dev_id，共享中断必须要传递一个非NULL的实参，和request_irq中的device_id保持一致


##### 为虚拟串口设备添加中断

```c
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

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"

#define VSER_IRQ_NR 37

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

static irqreturn_t vser_irq_handler(int irq, void *dev_id)
{
    //TODO：获取硬件的中断标志来判断该中断是否是本设备产生的，如果不是，则返回IRQ_NONE
    char data;
    get_random_bytes(&data, sizeof(data));
    data %= 26;
    data += 'A';
    if (!kfifo_is_full(&vsfifo))
    {
        if (!kfifo_in(&vsfifo, &data, sizeof(data)))
        {
            printk(KERN_ERR "vser: kfifo_in failure\n");
        }
    }
    if (!kfifo_is_empty(&vsfifo))
    {
        wake_up_interruptible(&vsdev.rwqh);
        kill_fasync(&vsdev.fapp, SIGIO, POLL_IN);
    }
    return IRQ_HANDLED;
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
    /* 向内核注册中断处理函数，共享中断，高电平触发 */
    ret = request_irq(VSER_IRQ_NR, vser_irq_handler, IRQF_TRIGGER_HIGH | IRQF_SHARED, "vser", &vsdev);
    if (ret)
    {
        goto irq_err;
    }
    return 0;
irq_err:
    cdev_del(&vsdev.cdev);
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
    free_irq(VSER_IRQ_NR, &vsdev);
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
```



#### 中断下半部

> Linux将中断分成了两部分：上半部和下半部，上半部完成紧急但能很快完成的事情，下半部完成不紧急但比较耗时的操作。下半部在执行的过程中，**中断被重新使能**，所以如果有新的硬件中断产生，将会停止执行下半部的程序，转为执行硬件中断的上半部。

* 软中断

> 软中断是中断下半部机制中的一种，描述软中断的结构是struct softirq_action，它的定义非常简单，就是内嵌了一个函数指针。内核共定义了NR_SOFTIRQS个struct softirq_action对象，这些对象被放在一个名叫softirq_vec的数组中，对象在数组中的下标就是这个软中断的编号。内核中有一个全局整型变量来记录是否有相应的软中断需要执行，如果需要，就回去检索softirq_vec这个数组，然后调用softirq_action对象中的action指针所指向的函数。内核中定义的软中断有：
>
> HI_SOFTIRQ，TIMER_SOFTIRQ，NET_TX_SOFTIRQ，NET_RX_SOFTIRQ，BLOCK_SOFTIRQ，BLOCK_IOPOLL_SOFTIRQ，TASKLET_SOFTIRQ，SCHED_SOFTIRQ，HRTIMER_SOFTIRQ，RCU_SOFTIRQ
>
> 虽然软中断可以实现中断的下半部，但是软中断基本上是内核开发者预定义好的，通常用在对性能要求特别高的场合。软中断最早执行的时间是中断上半部执行完成之后，但是中断还没有完全返回之前的时候。此外，内核还为每个CPU创建了一个软中断内核线程，当需要在中断处理函数之外执行软中断时，可以唤醒该内核线程。
>
> 最后需要注意，软中断也处于中断上下文中。

* tasklet

> 内核开发者专门保留了一个软中断给驱动开发者，它就是TASKLET_SOFTIRQ。驱动开发者要实现tasklet的下半部，就要构造一个struct tasklet_struct结构对象，并初始化里面的成员，然后放入对应CPU的tasklet链表中，最后设置软中断号TASKLET_SOFTIRQ所对应的比特位。

##### 用tasklet改写上面的中断程序

```c
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

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"

#define VSER_IRQ_NR 37

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

static void vser_tasklet(unsigned long arg);
/* 静态定义一个struct tasklet_struct结构对象 */
DECLARE_TASKLET(vstasklet, vser_tasklet, (unsigned long)&vsdev);

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

static irqreturn_t vser_irq_handler(int irq, void *dev_id)
{
    /* 将指定的struct tasklet_struct结构对象加入到对应CPU的tasklet链表中，下半部函数将会在未来的某个时间被调度 */
    tasklet_schedule(&vstasklet);
    return IRQ_HANDLED;
}

static void vser_tasklet(unsigned long arg)
{
    char data;
    get_random_bytes(&data, sizeof(data));
    data %= 26;
    data += 'A';
    if (!kfifo_is_full(&vsfifo))
    {
        if (!kfifo_in(&vsfifo, &data, sizeof(data)))
        {
            printk(KERN_ERR "vser: kfifo_in failure\n");
        }
    }
    if (!kfifo_is_empty(&vsfifo))
    {
        wake_up_interruptible(&vsdev.rwqh);
        kill_fasync(&vsdev.fapp, SIGIO, POLL_IN);
    }
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
    /* 向内核注册中断处理函数，共享中断，高电平触发 */
    ret = request_irq(VSER_IRQ_NR, vser_irq_handler, IRQF_TRIGGER_HIGH | IRQF_SHARED, "vser", &vsdev);
    if (ret)
    {
        goto irq_err;
    }
    return 0;
irq_err:
    cdev_del(&vsdev.cdev);
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
    free_irq(VSER_IRQ_NR, &vsdev);
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
```

#### 工作队列

> 不管是软中断还是tasklet，它们都有一个限制，就是在中断上下文中执行不能直接或间接地调用调度器。工作队列是另一种下半部机制，内核在启动的时候窗件一个或者多个(在多核处理器上)内核工作线程，工作线程取出工作队列中的每一个工作，然后执行，当队列中没有工作时，工作线程休眠。当驱动想要延迟某一个工作时，构造一个工作队列节点对象，然后加入到相应的工作队列，并唤醒工作队列，工作线程又取出队列上的节点来完成工作，所有工作完成后又休眠。因为是运行在进程上下文中，所以工作可以调用调度器。

#####用tasklet改写上面的中断程序

```c
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

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"

#define VSER_IRQ_NR 37

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

static void vser_work(struct work_struct *work);
/* 静态定义一个工作队列节点 */
DECLARE_WORK(vswork, vser_work);

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

static irqreturn_t vser_irq_handler(int irq, void *dev_id)
{
    /* 将工作节点加入到内核定义的全局工作队列中 */
    schedule_work(&vswork);
    return IRQ_HANDLED;
}

static void vser_work(struct work_struct *work)
{
    char data;
    get_random_bytes(&data, sizeof(data));
    data %= 26;
    data += 'A';
    if (!kfifo_is_full(&vsfifo))
    {
        if (!kfifo_in(&vsfifo, &data, sizeof(data)))
        {
            printk(KERN_ERR "vser: kfifo_in failure\n");
        }
    }
    if (!kfifo_is_empty(&vsfifo))
    {
        wake_up_interruptible(&vsdev.rwqh);
        kill_fasync(&vsdev.fapp, SIGIO, POLL_IN);
    }
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
    /* 向内核注册中断处理函数，共享中断，高电平触发 */
    ret = request_irq(VSER_IRQ_NR, vser_irq_handler, IRQF_TRIGGER_HIGH | IRQF_SHARED, "vser", &vsdev);
    if (ret)
    {
        goto irq_err;
    }
    return 0;
irq_err:
    cdev_del(&vsdev.cdev);
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
    free_irq(VSER_IRQ_NR, &vsdev);
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
```



### 延时控制

> 内核在启动过程中会计算一个全局变量**loops_per_jiffy**的值，该变量反映了一段循环延时的代码要循环多少次才能延时一个jiffy的时间。根据loops_per_jiffy这个值，就可以知道延时一微妙、一毫秒需要多少个循环。于是内核就根据此定义了一些靠循环来延时的宏或函数。
>
> `void ndelay(unsigned long x);udelay(n);mdelay(n);`
>
> 这些函数都是忙等延时，白白消耗CPU的时间，所以不推荐使用这些函数来延迟较长的时间。如果延时时间较长，又没有特殊的要求，那么可以使用休眠延时。
>
> `void msleep(unsigned int msecs);long msleep_interruptible(unsigned int msecs);void ssleep(unsigned int seconds);`



### 定时操作

##### 低分辨率定时器

> 经典的定时器是基于一个硬件定时器的，该定时器周期性地产生中断，产生中断的次数可以进行配置，内核源码中以HZ这个宏来代表这个配置值。也就是说，这个硬件定时器每秒钟会产生HZ次中断。该定时器自开机以来产生的中断次数会被记录在jiffies和jiffies_64全局变量中，内核通过链接器的帮助使jiffies和jiffies_64共享4个字节。
>
> 低分辨率定时器操作的的相关函数：
>
> init_timer：初始化一个定时器
>
> add_timer：将定时器添加到内核中的定时器链表中
>
> mod_timer：修改定时器的expires成员，而不考虑当前定时器的状态
>
> del_timer：从内核链表中删除该定时器，而不考虑当前定时器的状态
>
> 啮内核是在定时器中断的软中断下半部来处理这些定时器的，内核将会遍历链表中的定时器，如果当前的jiffies的值和定时器中的expires的值相等，那么定时器函数将会被执行，所以定时器函数是在中断上下文中执行的。另外，内核为了高效管理这些定时器，会将这些定时器按照超时时间进行分组，所以内核只会遍历快要到期的定时器。

* 为虚拟串口中添加低分辨率定时器

```c
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
#include <linux/timer.h>

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"

#define VSER_IRQ_NR 37

static DEFINE_KFIFO(vsfifo, char, 32);

struct vser_dev
{
    struct cdev cdev;
    unsigned int baud;
    struct option opt;
    wait_queue_head_t rwqh;     /* 读 等待队列头 */
    wait_queue_head_t wwqh;     /* 写 等待队列头 */
    struct fasync_struct *fapp; /* fasync_struct链表头 */
    struct timer_list timer;    /* 低分辨率定时器对象 */
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

static void vser_timer(unsigned long arg)
{
    char data;

    get_random_bytes(&data, sizeof(data));
    data %= 26;
    data += 'A';
    if (!kfifo_is_full(&vsfifo))
    {
        if (!kfifo_in(&vsfifo, &data, sizeof(data)))
        {
            printk(KERN_ERR "vser: kfifo_in failure\n");
        }
    }
    if (!kfifo_is_empty(&vsfifo))
    {
        wake_up_interruptible(&vsdev.rwqh);
        kill_fasync(&vsdev.fapp, SIGIO, POLL_IN);
    }
    /* 循环定时 */
    mod_timer(&vsdev.timer, get_jiffies_64() + msecs_to_jiffies(1000));
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
    /* 初始化定时器 */
    init_timer(&vsdev.timer);
    vsdev.timer.expires = get_jiffies_64() + msecs_to_jiffies(1000);
    vsdev.timer.function = vser_timer;
    vsdev.timer.data = (unsigned long)&vsdev;
    /* 向内核添加这个定时器 */
    add_timer(&vsdev.timer);
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

    /* 删除定时器 */
    del_timer(&vsdev.timer);
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
```

##### 高分辨率定时器

> 因为低分辨率定时器以jiffies来定时，所以定时精度受系统的HZ影响，而通常这个值都不高。对于声卡一类需要高精度定时的设备，这种精度是不能满足要求的，于是内核人员又开发了高分辨率定时器。高分辨率定时器是以ktime_t来定义时间的，ktime是一个共用体，如果是64位系统，那么时间用tv64来表示就可以了；如果是32位系统，那么时间分别用sec和nsec来表示秒和纳秒。

* 使用高分辨率定时器的虚拟串口驱动

```c
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

#define VSER_IRQ_NR 37

static DEFINE_KFIFO(vsfifo, char, 32);

struct vser_dev
{
    struct cdev cdev;
    unsigned int baud;
    struct option opt;
    wait_queue_head_t rwqh;     /* 读 等待队列头 */
    wait_queue_head_t wwqh;     /* 写 等待队列头 */
    struct fasync_struct *fapp; /* fasync_struct链表头 */
    struct hrtimer timer;       /* 高分辨率定时器对象 */
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

static enum hrtimer_restart vser_timer(struct hrtimer *timer)
{
    char data;

    get_random_bytes(&data, sizeof(data));
    data %= 26;
    data += 'A';
    if (!kfifo_is_full(&vsfifo))
    {
        if (!kfifo_in(&vsfifo, &data, sizeof(data)))
        {
            printk(KERN_ERR "vser: kfifo_in failure\n");
        }
    }
    if (!kfifo_is_empty(&vsfifo))
    {
        wake_up_interruptible(&vsdev.rwqh);
        kill_fasync(&vsdev.fapp, SIGIO, POLL_IN);
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
    init_waitqueue_head(&vsdev.wwqh);
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
```
