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



### IO内存

> IO内存的物理地址可以通过芯片手册来查询，但是在内核中应该使用虚拟地址，因此对这部分内存的访问必须要经过映射才行。

`struct resource* request_mem_region(start,n,name)`

* 创建一个从start开始的n字节物理内存资源，名字为name，并标记为忙状态，也就是向内核申请一段IO内存空间的使用权。在使用IO内存之前，一般先要用使用该函数进行申请，这样可以阻止其他驱动申请这块IO内存资源。创建的资源可以在`/proc/meminfo`文件中看到

`release_mem_region(start,n)`

* 释放之前申请的IO内存资源

`void __iomem* ioremap(phys_addr_t offset,unsigned long size)`

* 映射从offset开始的size字节IO内存，返回值为对应的虚拟地址 

`void iounmap(void __iomem* addr)`

* 解除之前的IO内存映射

`u8 readb(const volatile void __iomem* addr)`

`u16 readw(const volatile void __iomem* addr)`

`u32 readl(const volatile void __iomem* addr)`

* 分别按1字节、2字节和4字节读取映射之后地址为addr的IO内存，返回值为读取的IO内存内容

`void writeb(u8 b,volatile void __iomem* addr)`

`void writew(u16 b,volatile void __iomem* addr)`

`void writel(u32 b,volatile void __iomem* addr)`

* 分别按1字节、2字节和4字节将b写入映射之后地址为addr的IO内存

`ioread8(addr)`

`ioread16(addr)`

`ioread32(addr)`

`iowrite8(v,addr)`

`iowrite16(v,addr)`

`iowrite32(v,addr)`

`ioread8_rep(p,dst,count)`

`ioread16_rep(p,dst,count)`

`ioread32_rep(p,dst,count)`

`iowrite8_rep(p,src,count)`

`iowrite16_rep(p,src,count)`

`iowrite32_rep(p,src,count)`

* IO内存读写的另外一种形式，加"rep"的变体是连续读写count个单元

#### 简单LED驱动

```c
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

#define LED_MAJOR 256
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
```

* 应用层测试程序

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "led.h"

int main()
{
    int fd;
    int ret;
    int num = LED_RED;

    fd = open("/dev/led", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    while (1)
    {
        ret = ioctl(fd, LED_ON, num);
        if (ret == -1)
        {
            goto fail;
        }
        usleep(500000);
        ret = ioctl(fd, LED_OFF, num);
        if (ret == -1)
        {
            goto fail;
        }
        usleep(500000);

        num = (num + 1) % 3;
    }
    exit(EXIT_SUCCESS);
fail:
    perror("led test");
    exit(EXIT_FAILURE);
}
```



### DMA映射

> DMA映射的主要工作是找到一块适用于DMA操作的内存，返回其虚拟地址和总线地址，并关闭高速缓存，它是和体系结构相关的操作。

#### 一致性DMA映射

> 在ARM体系结构中，内核地址空间中的0xFFC00000到0xFFEFFFFF共计3MB的地址空间专门用于此类映射，它可以保证这段范围所映射的物理内存能够用于DMA操作，并且cache是关闭的。这种映射的优点是一旦DMA缓冲区建立，就再也不用担心cache的一致性问题，它通常适合于DMA缓冲区要存在于整个驱动程序的生命周期的情况。但是，这个缓冲区只有3MB，多个驱动都长期使用的话，最后会被分配完，导致其他驱动没有空间可以建立映射。

```c
/* 建立映射 */
#define dma_alloc_coherent(d,s,h,f) dma_alloc_attrs(d,s,h,f,NULL)
static inline void* dma_alloc_attrs(struct device* dev,size_t size,dma_addr_t* dma_handle,gfp_t flag,struct dma_attrs* attrs);
/* 释放映射 */
#define dma_free_coherent(d,s,c,h) dma_free_attrs(d,s,c,h,NULL)
static inline void dma_free_attrs(struct device* dev,size_t size,void* cpu_addr,dma_addr_t dma_handle,struct dma_attrs* attrs);
```

* dma_alloc_coherent返回用于DMA操作的内存(DMA缓冲区)虚拟地址
* 参数d是DMA设备
* 参数s是需要的DMA缓冲区大小，必须是**页大小的整数倍**
* 参数h是得到的DMA缓冲区的总线地址
* 参数f是内存分配掩码

#### 流式DMA映射

> 如果用于DMA操作的缓冲区不是驱动分配的，而是由内核的其他代码传递过来的，那么就需要进行流式DMA映射。流式DMA映射不会长期占用一致性映射的空间，并且开销比较小，所以一般推荐使用这种方式。但是流式DMA映射也会存在一个问题，那就是上层所给的缓冲区所对应的物理内存不一定可以用作DMA操作，在ARM体系结构中，只要能保证虚拟内存所对应的物理内存是连续的即可。
>
> 在ARM体系结构中，流式映射其实是通过使cache无效和写通操作来实现的，如果是读方向，那么DMA操作完成后，CPU在读内存之前只要操作cache，使这部分内存所对应的cache无效即可，这会导致CPU在cache中查找数据时cache不被命中，从而强制CPU到内存中去获取数据。对于写方向，则是设置为写通的方式，即保证CPU的数据会更新到DMA缓冲区。

```c
dma_map_single(d,a,s,r)
dma_unmap_single(d,a,s,r)
```

* 参数d是DMA设备
* 参数a是上层传递过来的缓冲区地址
* 参数s是大小
* 参数r是DMA传输方向
  * DMA_MEM_TO_MEM
  * DMA_MEM_TO_DEV
  * DMA_DEV_TO_MEM
  * DMA_DEV_TO_DEV



#### 分散/聚集映射

> 硬盘设备通常支持分散/聚集IO操作，例如readv和writev系统调用所产生的集群磁盘IO请求。对于写操作就是把虚拟地址分散的各个缓冲区的数据写入到磁盘，对于读操作就是把磁盘的数据读取到分散的各个缓冲区中。如果使用流式DMA映射，那就需要依次映射每一个缓冲区，DMA操作完成后再映射下一个。这会给驱动编程造成一些麻烦，如果能够一次映射多个分散的缓冲区，显然会方便很多，分散/聚集映射就是完成该任务的。
>
> 总的来说，分散/聚集映射就是一次性做多个流式DMA映射，为分散/聚集IO提供了较好的支持。

```c
int dma_map_sg(struct device* dev,struct scatterlist* sg,int nents,enum dma_data_direction dir);
void dma_unmap_sg(struct device* dev,struct scatterlist* sg,int nents,enum dma_data_direction dir);
```

* dev是DMA设备，sg是指向struct scatterlist类型数组的首元素的指针、数组中的每一个元素都描述了一个缓冲区，包括缓冲区对应的物理页框信息、缓冲区在物理页框中的偏移、缓冲区的长度和映射后得到的DMA总线地址等。nents是缓冲区的个数。dir是DMA的传输方向。
* dma_map_sg会遍历sg数组中的每一个元素，然后对每一个缓冲区做流式DMA映射。



#### DMA池

> 一致性DMA映射适合映射比较大的缓冲区，通常是页的整数倍大小，而较小的DMA缓冲区则用DMA池更适合。DMA 池和slab分配器的工作原理非常相似，就是预先分配一个大的DMA缓冲区，然后再在这个大的缓冲区中分配和释放较小的缓冲区。

`struct dma_pool* dma_pool_create(const char* name,struct device* dev,size_t size,size_t align,size_t boundary)`

* 创建DMA池，name是DMA池的名字，dev是DMA设备，size是DMA池的大小，align是对齐值，boundary是边界值，设为0则由大小和对齐值来自动决定边界。**该函数不能用于中断上下文**

`boid* dma_pool_alloc(struct dma_pool* pool,gfp_t mem_flags,dma_addr_t* handle)`

* 从DMA池pool中分配一块DMA缓冲区，mem_flags为分配掩码，handle是回传的DMA总线地址。函数返回虚拟地址

`void dma_pool_free(struct dma_pool* pool,void* vaddr,dma_addr_t dma)`

* 释放DMA缓冲区到DMA池pool中，vaddr是虚拟地址，dma是DMA总线地址

`void dma_pool_destroy(struct dma_pool* pool)`

* 销毁DMA池pool



### DMA统一编程接口(未尝试)

> 内核开发了一个统一的DMA子系统——dmaengine，它是一个软件框架，为上层的DMA应用提供了统一的编程接口，屏蔽了不同DMA控制器的细节。使用dmaengine完成DMA数据传输，基本需要一下几个步骤：
>
> 1. 分配一个DMA通道
> 2. 设置一些传输参数
> 3. 获取一个传输描述符
> 4. 提交传输
> 5. 启动所有挂起的传输，传输完成后回调函数被调用

##### 内存到内存的DMA传输实例

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

struct dma_chan *chan; //DMA通道
unsigned int *txbuf;   //DMA缓冲区虚拟地址
unsigned int *rxbuf;
dma_addr_t txaddr; //DMA缓冲区总线地址
dma_addr_t rxaddr;

static void dma_callback(void *data)
{
	int i;
	unsigned int *p = rxbuf;
	printk("dma complete\n");

	for (i = 0; i < PAGE_SIZE / sizeof(unsigned int); i++)
		printk("%d ", *p++);
	printk("\n");
}

static bool filter(struct dma_chan *chan, void *filter_param)
{
	/* 通过对比通道的名字来确定一个通道 */
	printk("%s\n", dma_chan_name(chan));
	return strcmp(dma_chan_name(chan), filter_param) == 0;
}

static int __init memcpy_init(void)
{
	int i;
	dma_cap_mask_t mask;
	struct dma_async_tx_descriptor *desc;
	char name[] = "dma2chan0";
	unsigned int *p;

	/* 先将掩码清零 */
	dma_cap_zero(mask);
	/* 设置掩码为DMA_MEMCPY，表示要获得一个能够完成内存到内存传输的DMA通道 */
	dma_cap_set(DMA_MEMCPY, mask);
	/* 获取一个DMA通道 */
	chan = dma_request_channel(mask, filter, name);
	if (!chan)
	{
		printk("dma_request_channel failure\n");
		return -ENODEV;
	}

	/* 建立两个DMA缓冲区的一致性映射，每个缓冲区为一页 */
	txbuf = dma_alloc_coherent(chan->device->dev, PAGE_SIZE, &txaddr, GFP_KERNEL);
	if (!txbuf)
	{
		printk("dma_alloc_coherent failure\n");
		dma_release_channel(chan);
		return -ENOMEM;
	}

	rxbuf = dma_alloc_coherent(chan->device->dev, PAGE_SIZE, &rxaddr, GFP_KERNEL);
	if (!rxbuf)
	{
		printk("dma_alloc_coherent failure\n");
		dma_free_coherent(chan->device->dev, PAGE_SIZE, txbuf, txaddr);
		dma_release_channel(chan);
		return -ENOMEM;
	}

	/* 初始化这两片内存，用于验证 */
	for (i = 0, p = txbuf; i < PAGE_SIZE / sizeof(unsigned int); i++)
		*p++ = i;
	for (i = 0, p = txbuf; i < PAGE_SIZE / sizeof(unsigned int); i++)
		printk("%d ", *p++);
	printk("\n");

	memset(rxbuf, 0, PAGE_SIZE);
	for (i = 0, p = rxbuf; i < PAGE_SIZE / sizeof(unsigned int); i++)
		printk("%d ", *p++);
	printk("\n");

	/* 创建内存到内存的传输描述符，指定目的地址，源地址，传输大小等 */
	desc = chan->device->device_prep_dma_memcpy(chan, rxaddr, txaddr, PAGE_SIZE, DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
	desc->callback = dma_callback;
	desc->callback_param = NULL;
	/* 提交DMA传输 */
	dmaengine_submit(desc);
	/* 发起DMA传输 */
	dma_async_issue_pending(chan);

	return 0;
}

static void __exit memcpy_exit(void)
{
	dma_free_coherent(chan->device->dev, PAGE_SIZE, txbuf, txaddr);
	dma_free_coherent(chan->device->dev, PAGE_SIZE, rxbuf, rxaddr);
	dma_release_channel(chan);
}

module_init(memcpy_init);
module_exit(memcpy_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("simple driver using dmaengine");
```

