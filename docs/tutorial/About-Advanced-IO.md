# About Advanced IO
### 高级IO操作

#### ioctl设备操作

> 为了处理设备非数据的操作(这些可以通过read、write接口来实现)，内核将对设备的控制操作委派给ioctl接口，ioctl也是一个系统调用，其函数原型为：`int ioctl(int fd,int request,...)`，与之对应的驱动接口函数是unlocked_ioctl和compat_ioctl，compat_ioctl是为了处理32位程序和64位内核兼容的一个函数接口，和体系结构相关。`long (*unlocked_ioctl)(struct file*,unsigned int,unsigned long)`
>
> 系统调用的过程：sys_ioctl首先调用了security_file_ioctl，然后调用了do_vfs_ioctl，在do_vfs_ioctl中先对一些特殊的命令进行了处理，再调用vfs_ioctl，在vfs_ioctl中最后调用了驱动的unblocked_ioctl。所以，**如果我们的命令定义和这些特殊的命令一样，那么我们驱动中的unblocked_ioctl就永远不会得到调用了**，这些特殊的命令有：FIOCLEX,FIONCLEX,FIONBIO,FIOASYNC,FIOQSIZE,FIFREEZE,FITHAW,FS_IOC_FIEMAP,FIGETBSZ

##### 命令编码规范

在内核文档Documentation/ioctl/ioctl-decoding.txt中有说明ioctl命令由四部分组成，内核提供了一组宏来定义、提取命令中的字段信息。

| 比特位   | 含义                                       |
| ----- | ---------------------------------------- |
| 31-30 | 00：命令不带参数；10：命令需要从驱动中获取数据，读方向；01：命令需要把数据写入驱动，写方向；11：命令既要写入数据又要获取数据，读写双向 |
| 29-16 | 如果命令带参数，则指定参数作战用的内存空间的大小                 |
| 15-8  | 每个驱动全局唯一的幻数(魔数)                          |
| 7-0   | 命令码                                      |

* 头文件(保存相关命令)

```c
#pragma once

struct option
{
    unsigned int datab;
    unsigned int parity;
    unsigned int stopb;
};

#define VS_MAGIC 's'

#define VS_SET_BAUD _IOW(VS_MAGIC, 0, unsigned int)
#define VS_GET_BAUD _IOR(VS_MAGIC, 1, unsigned int)
#define VS_SET_FMT _IOW(VS_MAGIC, 2, struct option)
#define VS_GET_FMT _IOR(VS_MAGIC, 3, struct option)
```

* 驱动源文件

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>

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
};

static struct vser_dev vsdev;

static int vser_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vser_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t vser_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    unsigned int copied = 0;
    /* 将内核FIFO中的数据取出，复制到用户空间 */
    int ret = kfifo_to_user(&vsfifo, buf, count, &copied);

    return ret == 0 ? copied : ret;
}

static ssize_t vser_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    unsigned int copied = 0;
    /* 将用户空间的数据放入内核FIFO中 */
    int ret = kfifo_from_user(&vsfifo, buf, count, &copied);

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

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
    .unlocked_ioctl = vser_ioctl,
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
```

* 应用层测试源文件

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "vser.h"

int main()
{
    int fd;
    int ret;
    unsigned int baud = 9600;
    struct option opt = {8, 1, 1};

    fd = open("/dev/vser0", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    ret = ioctl(fd, VS_SET_BAUD, &baud);
    if (ret == -1)
    {
        goto fail;
    }
    ret = ioctl(fd, VS_SET_FMT, &opt);
    if (ret == -1)
    {
        goto fail;
    }
    baud = 0;
    opt.datab = 0;
    opt.stopb = 0;
    opt.parity = 0;
    ret = ioctl(fd, VS_GET_BAUD, &baud);
    if (ret == -1)
    {
        goto fail;
    }
    ret = ioctl(fd, VS_GET_FMT, &opt);
    if (ret == -1)
    {
        goto fail;
    }

    printf("baud rate:%d\n", baud);
    printf("frame format:%d%c%d\n", opt.datab, opt.parity == 0 ? 'N' : opt.parity == 1 ? 'O' : 'E', opt.stopb);

    close(fd);
    exit(EXIT_SUCCESS);
fail:
    exit(EXIT_FAILURE);
}
```



#### proc文件操作

> proc文件系统是一种伪文件系统，只存在于内存中，只有在内核运行时才会动态生成里面的内容。这个文件系统通常挂载在/proc目录下，是内核开发者向用户导出信息的常用方式。可以使用该文件系统来对驱动进行调试，不过由于proc文件系统里的内容增多，已经不推荐这种方式。对硬件来讲，取而代之的是sysfs文件系统。不过某些时候，驱动开发者还是会使用这个接口，比如只想查看当前驱动的波特率和帧格式，而不想为之编写一个应用程序的时候，这也可以作为一种快速诊断故障的方案。

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>

#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"

struct vser_dev
{
	unsigned int baud;
	struct option opt;
	struct cdev cdev;
	struct proc_dir_entry *pdir; /* 指向/proc下的vser目录 */
	struct proc_dir_entry *pdat; /* 指向/proc/vser下的info文件 */
};

DEFINE_KFIFO(vsfifo, char, 32);
static struct vser_dev vsdev;

static int vser_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int vser_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t vser_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	int ret;
	unsigned int copied = 0;

	ret = kfifo_to_user(&vsfifo, buf, count, &copied);

	return ret == 0 ? copied : ret;
}

static ssize_t vser_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
	int ret;
	unsigned int copied = 0;

	ret = kfifo_from_user(&vsfifo, buf, count, &copied);

	return ret == 0 ? copied : ret;
}

static long vser_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != VS_MAGIC)
	{
		return -ENOTTY;
	}
	switch (cmd)
	{
	case VS_SET_BAUD:
		vsdev.baud = *(unsigned int *)arg;
		break;
	case VS_GET_BAUD:
		*(unsigned int *)arg = vsdev.baud;
		break;
	case VS_SET_FMT:
		if (copy_from_user(&vsdev.opt, (struct option __user *)arg, sizeof(struct option)))
		{
			return -EFAULT;
		}
		break;
	case VS_GET_FMT:
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

static struct file_operations vser_ops = {
	.owner = THIS_MODULE,
	.open = vser_open,
	.release = vser_release,
	.read = vser_read,
	.write = vser_write,
	.unlocked_ioctl = vser_ioctl,
};

static int show_data(struct seq_file *m, void *v)
{
	/* 私有数据在proc_create_data函数调用时传递的 */
	struct vser_dev *dev = m->private;
	/* 动态产生被读取的文件的内容 */
	seq_printf(m, "baudrate: %d\n", dev->baud);
	seq_printf(m, "frame format: %d%c%d\n", dev->opt.datab,
			   dev->opt.parity == 0 ? 'N' : dev->opt.parity == 1 ? 'O' : 'E',
			   dev->opt.stopb);
	return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_data, PDE_DATA(inode));
}

static struct file_operations proc_ops = {
	.owner = THIS_MODULE,
	.open = proc_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static int __init vser_init(void)
{
	int ret;
	dev_t dev;

	dev = MKDEV(VSER_MAJOR, VSER_MINOR);
	ret = register_chrdev_region(dev, VSER_DEV_CNT, VSER_DEV_NAME);
	if (ret)
		goto reg_err;

	cdev_init(&vsdev.cdev, &vser_ops);
	vsdev.cdev.owner = THIS_MODULE;
	vsdev.baud = 115200;
	vsdev.opt.datab = 8;
	vsdev.opt.parity = 0;
	vsdev.opt.stopb = 1;

	ret = cdev_add(&vsdev.cdev, dev, VSER_DEV_CNT);
	if (ret)
		goto add_err;

	/* 在/proc目录下建立vser目录，第二个参数为NULL表示在/proc目录下创建目录，第一个参数是目录的名字 */
	vsdev.pdir = proc_mkdir("vser", NULL);
	if (!vsdev.pdir)
		goto dir_err;
	/* 在vser目录下建立一个info文件，第一个参数是文件的名字，第二个参数是权限，第三个参数是目录的目录项指针，第四个参数是该文件的操作方法集合，第五个参数是该文件的私有数据 */
	vsdev.pdat = proc_create_data("info", 0, vsdev.pdir, &proc_ops, &vsdev);
	if (!vsdev.pdat)
		goto dat_err;

	return 0;

dat_err:
	remove_proc_entry("vser", NULL);
dir_err:
	cdev_del(&vsdev.cdev);
add_err:
	unregister_chrdev_region(dev, VSER_DEV_CNT);
reg_err:
	return ret;
}

static void __exit vser_exit(void)
{

	dev_t dev;

	dev = MKDEV(VSER_MAJOR, VSER_MINOR);

	remove_proc_entry("info", vsdev.pdir);
	remove_proc_entry("vser", NULL);

	cdev_del(&vsdev.cdev);
	unregister_chrdev_region(dev, VSER_DEV_CNT);
}

module_init(vser_init);
module_exit(vser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("virtual-serial");
```



#### 非阻塞型IO

> 如果应用程序以非阻塞的方式打开设备文件，当资源不可用时，驱动就应该立即返回，并用一个错误码EAGAIN来通知应用程序此时资源不可用，应用程序应该稍后再尝试。对于大多数时间内资源都不可用的设备(如鼠标、键盘)，这种尝试将会白白消耗CPU大量的时间。

* 驱动源码

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>

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
};

static struct vser_dev vsdev;

static int vser_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vser_release(struct inode *inode, struct file *filp)
{
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
    }
    /* 将内核FIFO中的数据取出，复制到用户空间 */
    ret = kfifo_to_user(&vsfifo, buf, count, &copied);

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
    }
    /* 将用户空间的数据放入内核FIFO中 */
    ret = kfifo_from_user(&vsfifo, buf, count, &copied);

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

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
    .unlocked_ioctl = vser_ioctl,
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
```

* 应用层测试源码

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "vser.h"

int main()
{
    int fd;
    int ret;
    unsigned int baud = 9600;
    struct option opt = {8, 1, 1};
    char rbuf[32] = {0};
    char wbuf[32] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

    fd = open("/dev/vser0", O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
        goto fail;
    }

    ret = read(fd, rbuf, sizeof(rbuf));
    if (ret < 0)
    {
        perror("read");
    }
    ret = write(fd, wbuf, sizeof(wbuf));
    if (ret < 0)
    {
        perror("first write");
    }
    ret = write(fd, wbuf, sizeof(wbuf));
    if (ret < 0)
    {
        perror("second write");
    }

    close(fd);
    exit(EXIT_SUCCESS);
fail:
    exit(EXIT_FAILURE);
}
```



#### 阻塞型IO

> 当进程以阻塞的方式打开设备文件时(默认方式)，如果资源不可用，那么进程阻塞，也就是进程睡眠。具体来讲就是，如果进程发现资源不可用，主动将自己的状态设置为TASK_UNINTERRUPTIBLE或者TASK_INTERRUPTIBLE，然后将自己加入一个驱动所维护的等待队列中，最后调用schedule主动放弃CPU，操作系统将之从运行队列上移除，并调度其他的进程执行。
>
> | 优点                                       | 缺点                |
> | ---------------------------------------- | ----------------- |
> | 资源不可用时，不占用CPU的时间，而非阻塞型IO必须要定期尝试，看看资源是否可以获得，这对于键盘和鼠标这类设备来将，其效率非常低 | 进程在休眠期间再也不能做其他的事情 |
>
> 要实现阻塞操作，最重要的数据结构就是等待队列，等待队列头的数据类型是wait_queue_head_t，队列节点的数据类型是wait_queue_t

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
    wait_queue_head_t rwqh; /* 读 等待队列头 */
    wait_queue_head_t wwqh; /* 写 等待队列头 */
};

static struct vser_dev vsdev;

static int vser_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vser_release(struct inode *inode, struct file *filp)
{
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
        if (wait_event_interruptible(vsdev.rwqh, !kfifo_is_empty(&vsfifo)))
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
        if (wait_event_interruptible(vsdev.wwqh, !kfifo_is_full(&vsfifo)))
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

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
    .unlocked_ioctl = vser_ioctl,
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
```



#### IO多路复用

> 可以同时监听多个设备的状态，如果被监听的所有设备都没有关心的事件产生，那么**系统调用被阻塞**。当被监听的任何一个设备有对应关心的事件发生，将会唤醒系统调用，系统调用将再次遍历所监听的设备，获取其事件信息，然后系统调用返回。之后可以对设备发起非阻塞的读或者写操作。

* poll系统调用

> 遍历所有被监听的设备的驱动中的poll接口函数，如果都没有关心的事件发生，那么poll系统调用休眠，直到至少有一个驱动唤醒它为止。驱动中的poll接口函数是不会休眠的，休眠发生在poll系统调用上，这和阻塞型IO不同。

* 驱动源码

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
    wait_queue_head_t rwqh; /* 读 等待队列头 */
    wait_queue_head_t wwqh; /* 写 等待队列头 */
};

static struct vser_dev vsdev;

static int vser_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vser_release(struct inode *inode, struct file *filp)
{
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
        if (wait_event_interruptible(vsdev.rwqh, !kfifo_is_empty(&vsfifo)))
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
        if (wait_event_interruptible(vsdev.wwqh, !kfifo_is_full(&vsfifo)))
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

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
    .unlocked_ioctl = vser_ioctl,
    .poll = vser_poll,
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
#include <poll.h>
#include <errno.h>
#include <linux/input.h>

#include "vser.h"

int main()
{
    int ret;
    struct pollfd fds[2];
    char rbuf[32];
    char wbuf[32];
    struct input_event key;

    fds[0].fd = open("/dev/vser0", O_RDWR | O_NONBLOCK);
    if (fds[0].fd == -1)
    {
        goto fail;
    }
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    /* 通过cat /proc/bus/input/devices来查看/dev/input目录下的event对应的设备 */
    /* 这里选择键盘 */
    fds[1].fd = open("/dev/input/event8", O_RDWR | O_NONBLOCK);
    if (fds[1].fd == -1)
    {
        goto fail;
    }
    fds[1].events = POLLIN;
    fds[1].revents = 0;

    while (1)
    {
        /* 第三个参数是毫秒的超时值，负数表示一直监听，直到被监听的文件描述符集合中的任意一个设备发生了事件才会返回 */
        ret = poll(fds, sizeof(fds), -1);
        if (ret == -1)
        {
            goto fail;
        }
        if (fds[0].revents & POLLIN)
        {
            ret = read(fds[0].fd, rbuf, sizeof(rbuf));
            if (ret < 0)
            {
                goto fail;
            }
            puts(rbuf);
        }
        if (fds[1].revents & POLLIN)
        {
            ret = read(fds[1].fd, &key, sizeof(key));
            if (ret < 0)
            {
                goto fail;
            }
            if (key.type == EV_KEY)
            {
                sprintf(wbuf, "%#x\n", key.code);
                ret = write(fds[0].fd, wbuf, strlen(wbuf) + 1);
                if (ret < 0)
                {
                    goto fail;
                }
            }
        }
    }
    exit(EXIT_SUCCESS);
fail:
    perror("poll test");
    exit(EXIT_FAILURE);
}
```



#### 异步IO(在Linux4.x的版本中发生重大变化，下面只介绍了旧的版本)

> 异步IO是POSIX定义的一组标准接口，调用者只是发起IO操作的请求，然后立即返回，程序可以去做别的事情。具体的IO操作在驱动中完成，驱动中可能会被阻塞，也可能不会被阻塞。当驱动的IO操作完成后，调用者将会得到通知，通常是内核向调用者发送信号或者自动调用调用者注册的回调函数，通知操作是由内核完成，而不是驱动本身。

* 驱动部分

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
#include <linux/aio.h>

#include "vser.h"

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 1
#define VSER_DEV_NAME "vser"

struct vser_dev
{
	unsigned int baud;
	struct option opt;
	struct cdev cdev;
	wait_queue_head_t rwqh;
	wait_queue_head_t wwqh;
};

DEFINE_KFIFO(vsfifo, char, 32);
static struct vser_dev vsdev;

static int vser_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int vser_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t vser_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	int ret;
	unsigned int copied = 0;

	if (kfifo_is_empty(&vsfifo))
	{
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible_exclusive(vsdev.rwqh, !kfifo_is_empty(&vsfifo)))
			return -ERESTARTSYS;
	}

	ret = kfifo_to_user(&vsfifo, buf, count, &copied);

	if (!kfifo_is_full(&vsfifo))
		wake_up_interruptible(&vsdev.wwqh);

	return ret == 0 ? copied : ret;
}

static ssize_t vser_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{

	int ret;
	unsigned int copied = 0;

	if (kfifo_is_full(&vsfifo))
	{
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (wait_event_interruptible_exclusive(vsdev.wwqh, !kfifo_is_full(&vsfifo)))
			return -ERESTARTSYS;
	}

	ret = kfifo_from_user(&vsfifo, buf, count, &copied);

	if (!kfifo_is_empty(&vsfifo))
		wake_up_interruptible(&vsdev.rwqh);

	return ret == 0 ? copied : ret;
}

static long vser_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) != VS_MAGIC)
		return -ENOTTY;

	switch (cmd)
	{
	case VS_SET_BAUD:
		vsdev.baud = arg;
		break;
	case VS_GET_BAUD:
		arg = vsdev.baud;
		break;
	case VS_SET_FFMT:
		if (copy_from_user(&vsdev.opt, (struct option __user *)arg, sizeof(struct option)))
			return -EFAULT;
		break;
	case VS_GET_FFMT:
		if (copy_to_user((struct option __user *)arg, &vsdev.opt, sizeof(struct option)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static unsigned int vser_poll(struct file *filp, struct poll_table_struct *p)
{
	int mask = 0;

	poll_wait(filp, &vsdev.rwqh, p);
	poll_wait(filp, &vsdev.wwqh, p);

	if (!kfifo_is_empty(&vsfifo))
		mask |= POLLIN | POLLRDNORM;
	if (!kfifo_is_full(&vsfifo))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static ssize_t vser_aio_read(struct kiocb *iocb, const struct iovec *iov, unsigned long nr_segs, loff_t pos)
{
	size_t read = 0;
	unsigned long i;
	ssize_t ret;

	/* 分多次来读，每次读iov_len长度，分别将读到的数据放入分散的内存区域中iov_base */
	for (i = 0; i < nr_segs; i++)
	{
		/* 异步IO可以在驱动中阻塞，但是上层的操作却是非阻塞的 */
		ret = vser_read(iocb->ki_filp, iov[i].iov_base, iov[i].iov_len, &pos);
		if (ret < 0)
			break;
		read += ret;
	}

	return read ? read : -EFAULT;
}

static ssize_t vser_aio_write(struct kiocb *iocb, const struct iovec *iov, unsigned long nr_segs, loff_t pos)
{
	size_t written = 0;
	unsigned long i;
	ssize_t ret;

	for (i = 0; i < nr_segs; i++)
	{
		ret = vser_write(iocb->ki_filp, iov[i].iov_base, iov[i].iov_len, &pos);
		if (ret < 0)
			break;
		written += ret;
	}

	return written ? written : -EFAULT;
}

static struct file_operations vser_ops = {
	.owner = THIS_MODULE,
	.open = vser_open,
	.release = vser_release,
	.read = vser_read,
	.write = vser_write,
	.unlocked_ioctl = vser_ioctl,
	.poll = vser_poll,
	.aio_read = vser_aio_read,
	.aio_write = vser_aio_write,
};

static int __init vser_init(void)
{
	int ret;
	dev_t dev;

	dev = MKDEV(VSER_MAJOR, VSER_MINOR);
	ret = register_chrdev_region(dev, VSER_DEV_CNT, VSER_DEV_NAME);
	if (ret)
		goto reg_err;

	cdev_init(&vsdev.cdev, &vser_ops);
	vsdev.cdev.owner = THIS_MODULE;
	vsdev.baud = 115200;
	vsdev.opt.datab = 8;
	vsdev.opt.parity = 0;
	vsdev.opt.stopb = 1;

	ret = cdev_add(&vsdev.cdev, dev, VSER_DEV_CNT);
	if (ret)
		goto add_err;

	init_waitqueue_head(&vsdev.rwqh);
	init_waitqueue_head(&vsdev.wwqh);

	return 0;

add_err:
	unregister_chrdev_region(dev, VSER_DEV_CNT);
reg_err:
	return ret;
}

static void __exit vser_exit(void)
{

	dev_t dev;

	dev = MKDEV(VSER_MAJOR, VSER_MINOR);

	cdev_del(&vsdev.cdev);
	unregister_chrdev_region(dev, VSER_DEV_CNT);
}

module_init(vser_init);
module_exit(vser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("A simple character device driver");
MODULE_ALIAS("virtual-serial");
```

* 驱动测试(编译链接是加上**-lrt**选项)

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
#include <aio.h>

#include "vser.h"

static void aiow_complete_handler(sigval_t sigval)
{
    int ret;
    struct aiocb *req = (struct aiocb *)sigval.sival_ptr;
    /* 获取io操作的错误码 */
    if (aio_error(req) == 0)
    {
        /* 获取io操作的返回值 */
        ret = aio_return(req);
        printf("aio write %d bytes\n", ret);
    }
    return;
}

static void aior_complete_handler(sigval_t sigval)
{
    int ret;
    struct aiocb *req = (struct aiocb *)sigval.sival_ptr;
    if (aio_error(req) == 0)
    {
        ret = aio_return(req);
        if (ret)
        {
            printf("aio read:%s\n", (char *)req->aio_buf);
        }
    }
    return;
}

int main()
{
    int ret;
    int fd;
    /* 定义读和写的异步IO控制块 */
    struct aiocb aiow, aior;

    fd = open("/dev/vser0", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    memset(&aior, 0, sizeof(aior));
    memset(&aiow, 0, sizeof(aiow));

    /* 初始化两个控制块 */
    aiow.aio_fildes = fd;
    aiow.aio_buf = malloc(32);
    strcpy((char *)aiow.aio_buf, "aio test");
    aiow.aio_nbytes = strlen((char *)aiow.aio_buf) + 1;
    aiow.aio_offset = 0;
    aiow.aio_sigevent.sigev_notify = SIGEV_THREAD;
    aiow.aio_sigevent.sigev_notify_function = aiow_complete_handler;
    aiow.aio_sigevent.sigev_notify_attributes = NULL;
    aiow.aio_sigevent.sigev_value.sival_ptr = &aiow;

    aior.aio_fildes = fd;
    aior.aio_buf = malloc(32);
    aior.aio_nbytes = 32;
    aior.aio_offset = 0;
    aior.aio_sigevent.sigev_notify = SIGEV_THREAD;
    aior.aio_sigevent.sigev_notify_function = aior_complete_handler;
    aior.aio_sigevent.sigev_notify_attributes = NULL;
    aior.aio_sigevent.sigev_value.sival_ptr = &aior;

    while (1)
    {
        /* 发起异步写操作，函数会立即返回，具体的写操作会在底层的驱动中完成 */
        if (aio_write(&aiow) == -1)
        {
            goto fail;
        }
        if (aio_read(&aior) == -1)
        {
            goto fail;
        }
        sleep(1);
    }
    exit(EXIT_SUCCESS);
fail:
    perror("async io test");
    exit(EXIT_FAILURE);
}
```



####异步通知

> 异步通知类似于异步IO，只是当设备资源可用时它是向应用层发信号，而不能直接调用应用层注册的回调函数，并且发信号的操作也是驱动程序自己来完成。这种机制和中断非常相似(信号其实相当于应用层的中断).
>
> 1. 注册信号处理函数，这相当于注册中断处理函数
> 2. 打开设备文件，设置文件属主，目的是使驱动根据打开文件的file结构，找到对应的进程，从而向该进程发送信号
> 3. 设置设备资源可用时驱动向进程发送的信号
> 4. 设置文件的FASYNC标志，使能异步通知机制，这相当于打开中断使能位

* 内核中关于异步通知的处理

> fcntl系统调用对应的内核函数是do_fcntl，do_fcntl会判断要执行的命令，如果是F_SETFL，则调用setfl函数，setfl函数会判断传递进来的flag是否包含FASYNC标志，如果是则调用驱动中的fasync接口函数，并传递FASYNC标志是否被设置。驱动中的fasync接口函数会调用fasync_helper函数，fasync_helper函数根据FASYNC标志是否被设置来决定在链表中添加一个struct fasync_struct节点还是删除一个节点。当资源可用时，驱动调用kill_fasync函数发送信号，该函数会遍历struct fasync_struct链表，从而找到所有要接收信号的进程，并调用send_sigio依次发送信号

* 驱动程序端

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
```

* 应用程序端

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
#include <poll.h>
#include <signal.h>

#include "vser.h"

int fd;

static void sigio_handler(int signum, siginfo_t *siginfo, void *act)
{
    int ret;
    char buf[32];
    if (signum == SIGIO)
    {
        /* si_band记录着资源是可读还是可写 */
        if (siginfo->si_band & POLLIN)
        {
            printf("FIFO is not empty\n");
            ret = read(fd, buf, sizeof(buf));
            if (ret != -1)
            {
                buf[ret] = '\0';
                puts(buf);
            }
        }
        if (siginfo->si_band & POLLOUT)
        {
            printf("FIFO is not full\n");
        }
    }
}

int main()
{
    int ret;
    int flag;

    struct sigaction newact, oldact;

    /* sigaction比signal更高级，主要是信号阻塞和提供信号信息这两方面 */
    /* 设置信号处理函数中要屏蔽的信号，防止嵌套 */
    sigemptyset(&newact.sa_mask);
    sigaddset(&newact.sa_mask, SIGIO);
    /* 注册信号函数 */
    newact.sa_flags = SA_SIGINFO;
    newact.sa_sigaction = sigio_handler;
    if (sigaction(SIGIO, &newact, &oldact) == -1)
    {
        goto fail;
    }

    fd = open("/dev/vser0", O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
        goto fail;
    }
    /* 设置文件属主，从而驱动可以根据file结构来找到对应的进程 */
    if (fcntl(fd, F_SETOWN, getpid()) == -1)
    {
        goto fail;
    }
    /* F_SETSIG不是POSIX标准，需要在编译时候加上-D_GNU_SOURCE */
    /* 当设备资源可用时，向进程发送SIGIO信号 */
    if (fcntl(fd, F_SETSIG, SIGIO) == -1)
    {
        goto fail;
    }
    /* 使能异步通知机制 */
    flag = fcntl(fd, F_GETFL);
    if (flag == -1)
    {
        goto fail;
    }
    /* 会触发调用驱动中的fasync接口函数 */
    if (fcntl(fd, F_SETFL, flag | FASYNC) == -1)
    {
        goto fail;
    }

    while (1)
    {
        sleep(1);
    }
    exit(EXIT_SUCCESS);
fail:
    perror("fasync test");
    exit(EXIT_FAILURE);S
}
```



#### mmap设备文件操作

> 显卡一类的设备有一片很大的显存，驱动程序将这片显存映射到内核的地址空间，应用程序没有权限直接访问。字符设备驱动提供了一个mmap接口，可以把内核空间中的那片内存所对应的物理地址空间再次映射到用户空间，这样一个物理内存就有了两份映射，或者说两个虚拟地址，一个在内核空间，一个在用户空间。这样就可以通过直接操作用户空间的这片映射之后的内存来直接访问物理内存，从而提高了效率。

* 虚拟帧缓存设备驱动

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

#define VFB_MAJOR 256
#define VFB_MINOR 1
#define VFB_DEV_CNT 1
#define VFB_DEV_NAME "vfbdev"

struct vfb_dev
{
    struct cdev cdev;
    unsigned char *buf;
};

static struct vfb_dev vfbdev;

static int vfb_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vfb_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vfb_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    /*
    ** vma用来描述一片映射区域的结构指针，一个进程有很多片映射的区域，每一个区域都有这样对应的一个结构，这些结构通过链表和红黑树组织在一起
    ** 第二个参数是用户指定的映射之后的虚拟起始地址
    ** 第三个参数是物理内存所对应的页框号(物理地址除以页的大小)
    ** 第四个参数是想要映射的空间的大小
    ** 最后一个参数代表该内存区域的访问权限
    ** 完了之后，这片物理内存区域将会被映射到用户空间
    */
    ret = remap_pfn_range(vma, vma->vm_start, virt_to_phys(vfbdev.buf) >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
    return ret ? -EAGAIN : 0;
}

static ssize_t vfb_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    int ret = 0;
    size_t len = (count > PAGE_SIZE) ? PAGE_SIZE : count;
    /* copy_to_user返回未复制成功的字节数 */
    ret = copy_to_user(buf, vfbdev.buf, len);
    return len - ret;
}

static struct file_operations vfb_ops = {
    .owner = THIS_MODULE,
    .open = vfb_open,
    .release = vfb_release,
    .read = vfb_read,
    .mmap = vfb_mmap,
};

/* 模块初始化函数 */
static int __init vfb_init(void)
{
    int ret;
    dev_t dev;
    unsigned long addr;

    dev = MKDEV(VFB_MAJOR, VFB_MINOR);
    /* 向内核注册设备号，静态方式 */
    ret = register_chrdev_region(dev, VFB_DEV_CNT, VFB_DEV_NAME);
    if (ret)
    {
        goto reg_err;
    }
    /* 初始化cdev对象，绑定ops */
    cdev_init(&vfbdev.cdev, &vfb_ops);
    vfbdev.cdev.owner = THIS_MODULE;
    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&vfbdev.cdev, dev, VFB_DEV_CNT);
    if (ret)
    {
        goto add_err;
    }

    /* 动态申请一页内存(内核空间按照页来管理内存，在进行映射时，地址要按照页大小对齐) */
    addr = __get_free_page(GFP_KERNEL); //内核空间的内存
    if (!addr)
    {
        goto get_err;
    }
    vfbdev.buf = (unsigned char *)addr;
    memset(vfbdev.buf, 0, PAGE_SIZE);

    return 0;
get_err:
    cdev_del(&vfbdev.cdev);
add_err:
    unregister_chrdev_region(dev, VFB_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit vfb_exit(void)
{
    dev_t dev;

    dev = MKDEV(VFB_MAJOR, VFB_MINOR);
    /* 释放申请的内存 */
    free_page((unsigned long)vfbdev.buf);
    cdev_del(&vfbdev.cdev);
    unregister_chrdev_region(dev, VFB_DEV_CNT);
}

/* 为模块初始化函数和清理函数取别名 */
module_init(vfb_init);
module_exit(vfb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("virtual-frame-buffer");
```

* 用户层代码

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main()
{
    int fd;
    int ret;
    char *start = NULL;
    int i;
    char buf[32]; //用户空间的内存

    fd = open("/dev/vfb0", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    /* 第一个参数是想要映射的起始地址，通常设为NULL，表示由内核来决定起始地址
    ** 第二个参数表示要映射的内存空间的大小
    ** 第三个参数表示映射后的空间是可读可写的
    ** 第四个参数表示该映射是多进程共享的 */
    start = mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (start == MAP_FAILED)
    {
        goto fail;
    }
    /* 对映射后的内存进行操作 */
    for (i = 0; i < 26; i++)
    {
        *(start + i) = 'a' + i;
    }
    *(start + i) = '\0';

    if (read(fd, buf, 27) == -1)
    {
        goto fail;
    }
    puts(buf);

    munmap(start, 32);

    exit(EXIT_SUCCESS);
fail:
    perror("mmap test");
    exit(EXIT_FAILURE);
}
```



#### 定位操作

> 支持随机访问的设备文件，访问的文件位置可以由用户来指定，并且对于读写这类操作，下一次访问的文件位置将会紧接在上一次访问结束的位置之后。需要注意的是大多数字符设备都不支持文件定位操作。

* 虚拟帧缓存设备驱动(支持文件定位)

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

#define VFB_MAJOR 256
#define VFB_MINOR 1
#define VFB_DEV_CNT 1
#define VFB_DEV_NAME "vfbdev"

struct vfb_dev
{
    struct cdev cdev;
    unsigned char *buf;
};

static struct vfb_dev vfbdev;

static int vfb_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vfb_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static int vfb_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int ret;
    /*
    ** vma用来描述一片映射区域的结构指针，一个进程有很多片映射的区域，每一个区域都有这样对应的一个结构，这些结构通过链表和红黑树组织在一起
    ** 第二个参数是用户指定的映射之后的虚拟起始地址
    ** 第三个参数是物理内存所对应的页框号(物理地址除以页的大小)
    ** 第四个参数是想要映射的空间的大小
    ** 最后一个参数代表该内存区域的访问权限
    ** 完了之后，这片物理内存区域将会被映射到用户空间
    */
    ret = remap_pfn_range(vma, vma->vm_start, virt_to_phys(vfbdev.buf) >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
    return ret ? -EAGAIN : 0;
}

static ssize_t vfb_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    int ret = 0;
    size_t len = count;
    /* 越界调整 */
    if (*pos + len > PAGE_SIZE)
    {
        len = PAGE_SIZE - *pos;
    }
    /* copy_to_user返回未复制成功的字节数 */
    ret = copy_to_user(buf, vfbdev.buf + *pos, len);
    *pos += len - ret;
    return len - ret;
}

static loff_t vfb_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t newpos;
    switch (whence)
    {
    case SEEK_SET:
        newpos = off;
        break;
    case SEEK_CUR:
        newpos = filp->f_pos + off;
        break;
    case SEEK_END:
        newpos = PAGE_SIZE + off;
        break;
    default:
        return -EINVAL;
    }
    /* 判断新位置是否合法 */
    if (newpos < 0 || newpos > PAGE_SIZE)
    {
        return -EINVAL;
    }
    /* 更新文件的位置值 */
    filp->f_pos = newpos;
    return newpos;
}

static struct file_operations vfb_ops = {
    .owner = THIS_MODULE,
    .open = vfb_open,
    .release = vfb_release,
    .read = vfb_read,
    .mmap = vfb_mmap,
    .llseek = vfb_llseek,
};

/* 模块初始化函数 */
static int __init vfb_init(void)
{
    int ret;
    dev_t dev;
    unsigned long addr;

    dev = MKDEV(VFB_MAJOR, VFB_MINOR);
    /* 向内核注册设备号，静态方式 */
    ret = register_chrdev_region(dev, VFB_DEV_CNT, VFB_DEV_NAME);
    if (ret)
    {
        goto reg_err;
    }
    /* 初始化cdev对象，绑定ops */
    cdev_init(&vfbdev.cdev, &vfb_ops);
    vfbdev.cdev.owner = THIS_MODULE;
    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&vfbdev.cdev, dev, VFB_DEV_CNT);
    if (ret)
    {
        goto add_err;
    }

    /* 动态申请一页内存(内核空间按照页来管理内存，在进行映射时，地址要按照页大小对齐) */
    addr = __get_free_page(GFP_KERNEL); //内核空间的内存
    if (!addr)
    {
        goto get_err;
    }
    vfbdev.buf = (unsigned char *)addr;
    memset(vfbdev.buf, 0, PAGE_SIZE);

    return 0;
get_err:
    cdev_del(&vfbdev.cdev);
add_err:
    unregister_chrdev_region(dev, VFB_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit vfb_exit(void)
{
    dev_t dev;

    dev = MKDEV(VFB_MAJOR, VFB_MINOR);
    /* 释放申请的内存 */
    free_page((unsigned long)vfbdev.buf);
    cdev_del(&vfbdev.cdev);
    unregister_chrdev_region(dev, VFB_DEV_CNT);
}

/* 为模块初始化函数和清理函数取别名 */
module_init(vfb_init);
module_exit(vfb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("virtual-frame-buffer");
```

* 用户层测试代码

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main()
{
    int fd;
    int ret;
    char *start = NULL;
    int i;
    char buf[32]; //用户空间的内存

    fd = open("/dev/vfb0", O_RDWR);
    if (fd == -1)
    {
        goto fail;
    }

    /* 第一个参数是想要映射的起始地址，通常设为NULL，表示由内核来决定起始地址
    ** 第二个参数表示要映射的内存空间的大小
    ** 第三个参数表示映射后的空间是可读可写的
    ** 第四个参数表示该映射是多进程共享的 */
    start = mmap(NULL, 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (start == MAP_FAILED)
    {
        goto fail;
    }
    /* 对映射后的内存进行操作 */
    for (i = 0; i < 26; i++)
    {
        *(start + i) = 'a' + i;
    }
    *(start + i) = '\0';

    if (lseek(fd, 3, SEEK_SET) == -1)
    {
        goto fail;
    }

    if (read(fd, buf, 10) == -1)
    {
        goto fail;
    }
    buf[10] = '\0';
    puts(buf);

    munmap(start, 32);

    exit(EXIT_SUCCESS);
fail:
    perror("mmap test");
    exit(EXIT_FAILURE);
}
```
