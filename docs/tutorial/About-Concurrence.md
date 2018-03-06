# About Concurrence
##互斥

#####中断屏蔽

> 竞态产生原因：一条内核执行路径被中断打断
>
> 解决办法：在访问共享资源之前，先将中断屏蔽，然后再访问共享资源，结束后再重新使能中断。使用local_irq_save和local_irq_restore来屏蔽和使能中断
>
> 注意：中断屏蔽的时间不宜过长

```c
unsigned long flags;
local_irq_save(flags);
i++;
local_irq_restore(flags);
```

##### 原子变量

> 原子变量其实是一个**整型变量**，对于整型变量，有的处理器专门提供一些指令来实现原子操作(如ARM处理器中的swp指令)，内核就会使用这些指令来对原子变量进行访问，但是有些处理器不具备这样的指令，内核就会使用其他手段来保证对它访问的原子性。

```c
atomic_t i = ATOMIC_INNIT(5);
atomic_inc(&i);
```

##### 自旋锁

> 在访问共享资源之前，首先要获得自旋锁，访问完共享资源后解锁，其他内核执行路径如果没有竞争到锁，只能忙等待，所以，自旋锁是一种忙等锁。所以获得自旋锁的临界代码段执行时间不宜过长。
>
> * 在获得锁的期间，不能够调用可能会引起进程切换的函数，因为这会增加持锁的时间，导致其他要获取锁的代码进行更长时间的等待。更糟糕的是，如果新调度的进程也要获取同样的自旋锁，那么会导致死锁。
>
>
> * 自旋锁是不可以递归的，即获得锁之后不能再获得自旋锁，否则会因为等待一个不能获得的锁而将自己锁死
> * 自旋锁可以用在中断上下文中，以为它是忙等锁，所以并不会引起进程的切换
> * 如果中断中也要访问共享资源，则在非中断处理代码中访问共享资源之前应该先禁止中断再获取自旋锁，使用spin_lock_irqsave来获取自旋锁和spin_lock_irqrestore来释放自旋锁

```c
int i = 5;
spinlock_t lock;
unsigned long flags;
spin_lock_init(&lock);
spin_lock_irqsave(&lock,flags);
i++;
spin_unlock_irqrestore(&lock,flags);
```

#####读写锁

> 在并发的方式中有读-读并发，读-写并发和写-写并发三种，很显然，一般的资源读操作并不会修改它的值(对某些读清零的硬件寄存器除外)，因此读和读之间是完全允许并发的。但是使用自旋锁，读操作也会被加锁，从而阻止了另外一个读操作。为了提高并发效率，必须要降低锁的粒度，以允许读和读之间的并发。为此，内核提供了一种允许读和读并发的锁，叫读写锁，其数据类型为rwlock_t

```c
int i = 5;
unsigned long flags;
rwlock_t lock;
rwlock_init(&lock);
write_lock_irqsave(&lock,falgs);
i++;
write_unlock_irqrestore(&lock,flags);
int v;
read_lock_irqsave(&lock,flags);
v = i;
read_unlock_irqrestore(&lock,flags);
```

##### 顺序锁

> 自旋锁不允许读和读之间的并发，读写锁则更进一步，允许读和读之间的并发，顺序锁又更进了一步，允许读和写之间的并发。顺序锁在读时不上锁，也就意味着在读的期间允许写，但是在读之前需要先读取一个顺序值，读操作完成后，再次读取顺序值，如果两者相等，说明在读的过程中没有发生过写操作，否则要重新读取。显然，写操作要上锁，并且要更新顺序值。顺序锁的数据类型是seqlock_t

```c
int i = 5;
unsigned long flags;
seqlock_t lock;
seqlock_init(&lock);
int v;
unsigned start;
do{
  start = read_seqbegin(&lock);
  v = i;
}while(read_seqretry(&lock,start));
write_seqlock_irqsave(&lock,flags);
i++;
write_sequnlock_irqrestore(&lock,flags);
```

##### 信号量

> 锁机制有一个限制，那就是在锁获得期间不能调用调度器，即不能引起进程切换。但是内核中有很多函数都可能会触发对调度器的调用。另外，对于忙等锁来说，当临界代码段执行的时间比较长的时候，会降低系统的效率。为此，内核提供了一种叫信号量的机制来取消这一限制。进程在不能获得信号量的情况下会休眠，不会忙等待，从而适用于临界代码段运行时间比较长的情况。获取信号量的操作可能会引起进程切换，所以不能用在中断上下文中，如果必须要使用，也只能使用down_trylock。不过在中断上下文中可以使用up释放信号量，从而唤醒其他进程。
>
> ```c
> struct semaphore{
>   raw_spinlock_t lock;
>   unsigned int count;
>   struct list_head wait_list;
> };
> ```
>
> 其中的count成员用来记录信号量资源的情况，当count的值不为0时是可以获得信号量的，当count的值为0时信号量就不能被获取。
>
> **信号量的开销比较大，在不违背自旋锁的使用规则的情况下，优先使用自旋锁**
>
> 和相对于自旋锁的读写锁类似，也有相对于信号量的读写信号量，他和信号量的本质是一样的，只是将读和写分开了，从而能获取更好的并发性能。读写信号量的结构类型是struct rw_semaphore

```c
struct semaphore sem;
sema_init(&sem,1);
if(down_interruptible(&sem)){
  return -ERESTARTSYS;//被信号唤醒
}
//TODO,对共享资源进行访问，执行一些耗时操作或者可能会引起进程调度的操作
up(&sem);
```

##### 互斥量

> 信号量除了不能用于中断上下文，还有一个缺点就是不是很智能。在获取信号量的代码中，只要信号量的值为0，进程马上就休眠了。但是更一般的情况是，在不会等待太长的时间后，信号量就可以马上获得，那么信号量的操作就要经历进程先休眠再被唤醒的一个漫长过程。可以在信号量不能获取的时候，稍微耐心等待一小段时间，如果这段时间能够获取信号量，那么获取信号量的操作就可以立即返回，否则再将进程休眠也不迟。为了实现这种比较智能化的信号量，内核提供了另外一种专门用于互斥的高效信号量，也就是互斥量，类型为：struct mutex
>
> 要在同一上下文中对互斥量上锁和解锁。和自旋锁一样，互斥量的上锁是不能递归的。当持有互斥量的时候不能退出进程。**不能在中断上下文中使用互斥量**。持有互斥量期间，可以调用可能会引起进程切换的函数。**在不违背自旋锁的使用规则时，应该优先使用自旋锁。**
>
> **在不能使用自旋锁但不为违背互斥量的使用规则时，应该优先使用互斥量**

```c
int i = 5;
struct mutex lock;
mutex_init(&lock);
mutex_lock(&lock);
i++;
mutex_unlock(&lock);
```

##### RCU(Read Copy Update)机制

> RCU机制对共享资源的访问都是通过指针来进行的，读者(对共享资源发起读访问操作的代码)通过对该指针进行解引用，来获取想要的数据。写者在发起写访问操作的时候，并不是去写以前的共享资源内存，而是另起炉灶，重新分配一片内存空间，复制以前的数据到新开辟的内存空间，然后修改新分配内存空间里面的内容。当写结束后，等待所有的读者都完成了对原有内存空间的读取后，将读的指针更新，指向新的内存空间，之后的读操作将会得到更新后的数据。**这非常适合用于读访问多，写访问少的情况，它尽可能地减少了对锁的使用**。

#### 虚拟串口加入互斥机制

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
```



### 归纳：在驱动中如何解决静态的问题

1. 找出驱动中的共享资源，比如available和接收fifo
2. 考虑在驱动中的什么地方有并发的可能、是何种并发，以及由此引起的竞态
3. 使用何种手段互斥。互斥的手段有很多种，应该尽量选用一些简单的、开销小的互斥手段，当该互斥手段受到某条使用规则的制约后再考虑其他的互斥手段



## 同步

###完成量

> 同步是指内核中的执行路径需要按照一定的顺序来进行。对于一个ADC设备来说，假设一个驱动中的一个执行路径是将ADC采集到的数据做某些转换操作(比如将若次采样结果做平均)，而另一个执行路径专门负责ADC采样，那么做转换操作的执行路径要等待做采样的执行路径。
>
> 同步可以用信号量来实现，就上面的ADC驱动来说，可以先初始化一个值为0的信号量，做转换操作的执行路径先用down来获取这个信号量，如果在这之前没有采集到数据，那么做转换操作的路径将会休眠等待。当做采样的路径完成采样后，调用up释放信号量，那么做转换操作的执行路径将会被唤醒。
>
> 不过内核专门提供了一个**完成量**来实现该操作：
>
> ```c
> struct completion{
>   unsigned int done;
>   wait_queue_head_t wait;
> };
> ```
>
> 其中done是是否完成的状态，是一个计数值，为0表示未完成。wait是一个等待队列头。当done为0时进程阻塞，当内核的其他执行路径使done的值大于0时，负责唤醒被阻塞在这个完成量上的进程。

```c
//定义完成量
struct completion comp;
//使用前初始化完成量
init_completion(&comp);
//等待其他任务完成某个操作
wait_for_completion(&comp);

//某个操作完成，唤醒等待的任务
complete(&comp);
```



