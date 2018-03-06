# About Memory && DMA

### 内存组织

> 在Linux的内存组织中，将最高层次定义为内存节点，当要分配一块内存时，应该先考虑从CPU的本地内存对应的节点来分配内存，如果不能满足要求，再考虑从非本地内存的节点上分配内存。
>
> Linux内存管理的第二个层次为区，每个内存节点都划分为多个区，比如有：ZONE_DMA，ZONE_NORMAL，ZONE_HIGHMEM。
>
> Linux内存管理的第三个层次为页，对物理内存而言，通常叫做页帧或页框。页的大小由CPU的内存管理单元MMU来决定，最常见的有大小为4KB。Linux用struct page来表示一个物理页。Linux内核是按照页来管理内存的，最基本的内存分配和释放都是按页进行的。

### 按页分配内存

`struct page* alloc_pages(gfp_t gfp_mask,unsigned int order)`

- 分配2的order次方连续的物理页，返回起始页的struct page对象地址
- 参数gfp_mask用于控制页面分配行为的掩码值
  - GFP_ATOMIC，告诉分配器以原子的方式分配内存，即在内存分配期间不能够引起进程切换。比如在中断上下文和持有自旋锁的上下文中，必须使用该掩码来获取内存
  - GFP_KERNEL，最常用的内存分配掩码，在内存分配过程中允许进程切换，可以进行IO操作(比如为了得到空闲页，将页面暂时换出到磁盘上)，允许执行文件系统操作
  - GFP_USER，用于为用户空间分配内存，在内存分配过程中允许进程切换
  - GFP_DMA，告诉分配器只能在ZONE_DMA区分配内存，用于DMA操作
  - __GFP_HIGHMEM，在高端内存区域分配物理内存

`alloc_page（gfp_mask)`

* 分配单独的一页内存，也就是说order为0的alloc_pages函数的封装

`__free_pages(struct page* page,unsigned int order)`

* 释放这些分配得到的页

> 上面的额函数返回的都是管理物理页面的struct page对象地址，而我们通常需要的是物理内存在内核中对应的虚拟地址。对于不是高端内存的物理地址，虚拟地址和它有一个固定的偏移.但是对于高端内存的虚拟地址的获取比较麻烦，内核需要操作页表来建立映射。

`void* page_address(const struct_page* page)`

* 用于非高端内存的虚拟地址的获取，返回物理页对应的内核空间的虚拟地址

`void* kmap(struct page* page)`

* 用于返回分配的高端或非高端内存的虚拟地址，如果不是高端内存，则内部调用的其实是page_address。*该函数可能会引起休眠*

`void* kmap_atomic(struct page* page)`

* 和kmap功能类似，但操作是原子性的，也叫临时映射

`void* kunmap(struct page* page)`

* 解除kmap和kmap_atomic的映射

> 内存分配都要分两步来完成，首先获取物理内存页，然后再返回内核的虚拟地址。内核也提供了合二为一的函数。如下所示，这几个函数**不能在高端内存上分配页面**

`unsigned long __get_free_pages(gfp_t gfp_mask,unsigned int order)`

`unsigned long __get_free_page(gfp_t gfp_mask)`

`unsigned long get_zeroed_page(gfp_t gfp_mask)`

`void free_pages(unsigned long addr,unsigned int order)`

`void free_page(unsigned long addr)`

### slab分配器

> 驱动中更多情况是需要动态分配若干个字节，页分配器不能提供较好的支持。于是内核设计出了针对小块内存的分配器——slab分配器。其设计思想是：首先使用页分配器预先分配若干个页，然后将这若干个页按照一个特定的对象大小进行切分，要获取一个对象，就从这个对象的高速缓存中来获取，使用完后，再释放到同样的对象高速缓存中。所以slab分配器不仅具有能够管理小块内存的能力，同时也提供类似于高速缓存的作用，使对象的分配和释放变得非常迅速。

`struct kmem_cache* kmem_cache_create(const char* name,size_t size,size_t align,unsigned long flags,void (*ctor)(void*))`

* 创建一个高速缓存，第一个参数是高速缓存的名字，可以再/proc/slabinfo文件中查看到该信息，第二个参数是对象的大小，第三个参数是slab内第一个对象的偏移，也就是说对齐设置，通常为0即可，第四个参数是可选的设置项，用于进一步控制slab分配器，最后一个参数是追加新的页到高速缓存中用到的构造函数，通常不需要

`void* kmem_cache_alloc(struct kmem_cache* cachep,gfp_t flags)`

* 从对象高速缓存cachep中返回一个对象，flags是分配的掩码

`void kmem_cache_free(struct kmem_cache* cachep,void* objp)`

* 释放一个对象到高速缓存cachep中

`void kmem_cache_destory(struct kmem_cache* cachep)`

* 销毁对象高速缓存

```c
#include <linux/slab.h>

struct test{
  char c;
  int i;
};
static struct kmem_cache* test_cache;
static struct test* t;

/* 创建高速缓存，每个对象的大小为sizeof(struct test) */
test_cache = kmem_cache_create("test_cache",sizeof(struct test),0,0,NULL);
if(!test_cache){
  return -ENOMEM;
}
/* 分配一个对象 */
t = kmem_cache_alloc(test_cache,GFP_KERNEL);
if(!t){
  return -ENOMEM;
}
/* 释放对象 */
kmem_cache_free(test_cache,t);
/*销毁高速缓存 */
kmem_cache_destory(test_cache);
```

> 上面的API可以快速获取和释放一个若干字节的对象，但是在驱动中更多的是像在应用程序中使用malloc和free一样，来简单、快速分配和释放若干个字节，为此内核专门创建了一些常见字节大小的对象高速缓存。通过`cat /proc/slabinfo | grep kmalloc`可以观察到这一点。

```shell
morris@morris:~$ sudo cat /proc/slabinfo  | grep kmalloc
dma-kmalloc-8192       0      0   8192    4    8 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-4096       0      0   4096    8    8 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-2048       0      0   2048   16    8 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-1024       0      0   1024   32    8 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-512       32     32    512   32    4 : tunables    0    0    0 : slabdata      1      1      0
dma-kmalloc-256        0      0    256   32    2 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-128        0      0    128   32    1 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-64         0      0     64   64    1 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-32         0      0     32  128    1 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-16         0      0     16  256    1 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-8          0      0      8  512    1 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-192        0      0    192   21    1 : tunables    0    0    0 : slabdata      0      0      0
dma-kmalloc-96         0      0     96   42    1 : tunables    0    0    0 : slabdata      0      0      0
kmalloc-8192         120    120   8192    4    8 : tunables    0    0    0 : slabdata     30     30      0
kmalloc-4096         188    224   4096    8    8 : tunables    0    0    0 : slabdata     28     28      0
kmalloc-2048         411    464   2048   16    8 : tunables    0    0    0 : slabdata     29     29      0
kmalloc-1024        1779   1920   1024   32    8 : tunables    0    0    0 : slabdata     60     60      0
kmalloc-512         1448   1568    512   32    4 : tunables    0    0    0 : slabdata     49     49      0
kmalloc-256         8829  10048    256   32    2 : tunables    0    0    0 : slabdata    314    314      0
kmalloc-192         2520   2520    192   21    1 : tunables    0    0    0 : slabdata    120    120      0
kmalloc-128         1369   1632    128   32    1 : tunables    0    0    0 : slabdata     51     51      0
kmalloc-96          5040   5040     96   42    1 : tunables    0    0    0 : slabdata    120    120      0
kmalloc-64         19743  20224     64   64    1 : tunables    0    0    0 : slabdata    316    316      0
kmalloc-32         12669  14720     32  128    1 : tunables    0    0    0 : slabdata    115    115      0
kmalloc-16          5120   5120     16  256    1 : tunables    0    0    0 : slabdata     20     20      0
kmalloc-8           4608   4608      8  512    1 : tunables    0    0    0 : slabdata      9      9      0
```

> 内核预先创建了很多8字节、16字节等大小的对象高速缓存，可以使用如下API来分配和释放这些对象

```c
void* kmalloc(size_t size,gfp_t flags);
void* kzalloc(size_t size,gfp_t flags);
void kfree(const void*);
```



### 不连续内存页分配

> 内存分配函数都能保证所分配的内存在物理地址空间是连续的，但是这个特点使得想要分配大块的内存变得困难。因为频繁的分配和释放将会导致碎片的产生，为此内核地址空间分配了专门的一片区域vmalloc，通过对页表进行操作，可以把vmalloc中的一部分映射到物理地址不连续的页上面。这样可以使连续的内核虚拟地址对应不连续的物理地址，从而可以组合不连续的页，得到较大的内存。当然，因为要操作页表，所以它的工作效率不高，不适合频繁地分配和释放。

```c
void* vmalloc(unsigned long size);
void* vzalloc(unsigned long size);
void vfree(const void* addr);
```

* 需要注意，**vmallc和vzalloc函数可能会引起进程休眠**。



### per-CPU变量

> per-CPU变量就是每个CPU有一个变量的副本，一个典型的用法就是统计各个CPU上的一些信息。例如每个CPU上可以保存一个运行在该CPU上的进程的数量，要统计整个系统的进程数，则可以将每个CPU上的进程数相加。

```c
DEFINE_PER_CPU(type,name)//定义一个类型为type，名字为name的per-CPU变量
DECLARE_PER_CPU(type,name)//声明在别的地方定义的per-CPU变量
get_cpu_var(var)//禁止内核抢占，获得当前处理器上的变量var
put_cpu_var(var)//重新使能内核抢占
per_cpu(var,cpu)//获取其他cpu上的变量var
alloc_percpu(type)//动态分配一个类型为type的per-CPU变量
void free_percpu(void __percpu* __pdata);//释放动态分配的per-CPU变量
for_each_possible_cpu(cpu)//遍历每一个可能的CPU，cpu是获得的CPU编号
```

* 统计总进程数

```c
DEFINE_PER_CPU(unsigned long,process_counts) = 0;
int nr_process(void)
{
  int cpu;
  int total = 0;
  for_each_possible_cpu(cpu)
    total += per_cpu(process_counts,cpu);
  return total;
}
```



##### 使用动态内存改写虚拟串口驱动

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

struct vser_dev
{
    struct cdev cdev;
    unsigned int baud;
    struct option opt;
    wait_queue_head_t rwqh;     /* 读 等待队列头 */
    struct fasync_struct *fapp; /* fasync_struct链表头 */
    struct hrtimer timer;       /* 高分辨率定时器对象 */
    atomic_t available;         /* 当前串口是否可用 */
    struct kfifo fifo;
};

static struct vser_dev *vsdev;
static int vser_fasync(int fd, struct file *filp, int on);

static int vser_open(struct inode *inode, struct file *filp)
{
    if (atomic_dec_and_test(&vsdev->available))
    {
        /* 首次被打开 */
        return 0;
    }
    else
    {
        /* 竞争失败的进程应该要将available的值加回来，否则以后串口将永远无法打开 */
        atomic_inc(&vsdev->available);
        return -EBUSY;
    }
}

static int vser_release(struct inode *inode, struct file *filp)
{
    /* 将节点从链表删除，这样进程不会再收到信号 */
    vser_fasync(-1, filp, 0);
    /* 使用完串口之后要把available加回来，使串口又可用 */
    atomic_inc(&vsdev->available);
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
    spin_lock(&vsdev->rwqh.lock);
    /* 判断fifo是否为空 */
    if (kfifo_is_empty(&vsdev->fifo))
    {
        /* 如果是以非阻塞的方式打开 */
        if (filp->f_flags & O_NONBLOCK)
        {
            spin_unlock(&vsdev->rwqh.lock);
            return -EAGAIN;
        }
        /* 如果是阻塞方式打开 */
        /* 进程睡眠，直到fifo不为空或者进程被信号唤醒 */
        /* wait_event_interruptible_locked会在进程休眠前自动释放自旋锁，醒来后又将重新获得自旋锁 */
        if (wait_event_interruptible_locked(vsdev->rwqh, !kfifo_is_empty(&vsdev->fifo)))
        {
            spin_unlock(&vsdev->rwqh.lock);
            /* 被信号唤醒 */
            return -ERESTARTSYS;
        }
    }
    /* 将内核FIFO中的数据取出，复制到临时缓冲区 */
    len = kfifo_out(&vsdev->fifo, rbuf, len);
    /* 释放自旋锁 */
    spin_unlock(&vsdev->rwqh.lock);

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
        vsdev->baud = *(unsigned int *)arg;
        break;
    case VS_GET_BAUD:
        *(unsigned int *)arg = vsdev->baud;
        break;
    case VS_SET_FMT: /* 设置帧格式 */
        /* 返回未复制成功的字节数，该函数可能会使进程睡眠 */
        if (copy_from_user(&vsdev->opt, (struct option __user *)arg, sizeof(struct option)))
        {
            return -EFAULT;
        }
        break;
    case VS_GET_FMT:
        /* 返回未复制成功的字节数，该函数可能会使进程睡眠 */
        if (copy_to_user((struct option __user *)arg, &vsdev->opt, sizeof(struct option)))
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
    poll_wait(filp, &vsdev->rwqh, p);

    spin_lock(&vsdev->rwqh.lock);
    /* 根据资源的情况返回设置mask的值并返回 */
    if (!kfifo_is_empty(&vsdev->fifo))
    {
        mask |= POLLIN | POLLRDNORM;
    }
    spin_unlock(&vsdev->rwqh.lock);
    return mask;
}

static int vser_fasync(int fd, struct file *filp, int on)
{
    /* 调用fasync_helper来构造struct fasync_struct节点，并加入到链表 */
    return fasync_helper(fd, filp, on, &vsdev->fapp);
}

static enum hrtimer_restart vser_timer(struct hrtimer *timer)
{
    unsigned char data;

    get_random_bytes(&data, sizeof(data));
    data %= 26;
    data += 'A';
    spin_lock(&vsdev->rwqh.lock);
    if (!kfifo_is_full(&vsdev->fifo))
    {
        if (!kfifo_in(&vsdev->fifo, &data, sizeof(data)))
        {
            printk(KERN_ERR "vser: kfifo_in failure\n");
        }
    }
    if (!kfifo_is_empty(&vsdev->fifo))
    {
        spin_unlock(&vsdev->rwqh.lock);
        wake_up_interruptible(&vsdev->rwqh);
        kill_fasync(&vsdev->fapp, SIGIO, POLL_IN);
    }
    else
    {
        spin_unlock(&vsdev->rwqh.lock);
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

    vsdev = kzalloc(sizeof(struct vser_dev), GFP_KERNEL);
    if (!vsdev)
    {
        ret = -ENOMEM;
        goto mem_err;
    }
    ret = kfifo_alloc(&vsdev->fifo, VSRE_FIFO_SIZE, GFP_KERNEL);
    if (ret)
    {
        goto fifo_err;
    }

    /* 初始化cdev对象，绑定ops */
    cdev_init(&vsdev->cdev, &vser_ops);
    vsdev->cdev.owner = THIS_MODULE;
    vsdev->baud = 115200;
    vsdev->opt.datab = 8;
    vsdev->opt.stopb = 1;
    vsdev->opt.parity = 0;
    /* 初始化等待队列头 */
    init_waitqueue_head(&vsdev->rwqh);
    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&vsdev->cdev, dev, VSER_DEV_CNT);
    if (ret)
    {
        goto add_err;
    }
    /* 初始化定时器 */
    /* CLOCK_MONOTONIC时钟类型表示自系统开机以来的单调递增时间,HRTIMER_MODE_REL表示相对时间模式 */
    hrtimer_init(&vsdev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    vsdev->timer.function = vser_timer;
    /* 启动定时器 */
    hrtimer_start(&vsdev->timer, ktime_set(1, 1000), HRTIMER_MODE_REL);
    /* 将原子变量赋值为1 */
    atomic_set(&vsdev->available, 1);
    return 0;

add_err:
    kfifo_free(&vsdev->fifo);
fifo_err:
    kfree(vsdev);
mem_err:
    unregister_chrdev_region(dev, VSER_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit vser_exit(void)
{
    dev_t dev;

    /* 关闭定时器 */
    hrtimer_cancel(&vsdev->timer);
    dev = MKDEV(VSER_MAJOR, VSER_MINOR);
    cdev_del(&vsdev->cdev);
    unregister_chrdev_region(dev, VSER_DEV_CNT);
    kfifo_free(&vsdev->fifo);
    kfree(vsdev);
}

/* 为模块初始化函数和清理函数取别名 */
module_init(vser_init);
module_exit(vser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("virtual-serial");
```

