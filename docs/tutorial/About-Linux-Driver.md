# About Linux Driver
### 内核模块

> Windows是微内核操作系统，微内核只是先内核中相当关键和核心的部分，其他功能模块被单独编译，功能模块之间的交互需要通过微内核提供的某种通信机制来建立。
>
> Linux是宏内核操作系统，所有的内核功能都被整体编译在一起，形成一个单独的内核镜像文件。其显著特点是效率非常高，内核中各功能模块是通过直接的函数调用来进行的。
>
> 为了方便驱动开发，Linux引入了内核模块，它是可以被单独编译的内核代码，可以在需要的时候动态加载到内核，从而动态地增加内核的功能。在不需要的时候可以动态地卸载，从而减少内核的功能，并节约一部分内存。内核模块的这一特点也有助于减小内核镜像文件的体积，自然也就减少了内核所占用的内存空间。

#### 最简单的内核模块

```c
#include <linux/init.h>//包含init_module和cleanup_module的函数原型声明
#include <linux/kernel.h>//包含printk函数原型的声明
#include <linux/module.h>//包含MODULE_LICENSE等宏定义

/* 模块初始化函数 */
static int __init simple_init(void){
    printk("module init\n");
    return 0;
}

/* 模块清理函数 */
static void __exit simple_exit(void){
    printk("cleanup module\n");
}

/* 为模块初始化函数和清理函数取别名 */
module_init(simple_init);
module_exit(simple_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("first_module");
```

* `MODULE_LICENSE`用来表征该模块接受的许可证协议，常见的有：GPL，GPL v2，Dual BSD/GPL，Dual MIT/GPL，Dual MPL/GPL等
* `MODULE_LICENSE`宏将会生成一些模块信息，放在ELF文件中的一个特殊的段中，模块在加载的时候会将该信息复制到内存中，并检查该信息。如果一个模块没有声明接受的许可协议，那么内核中的某些功能(API接口)是无法被调用的
* 模块的初始化函数(`init_module`)和清除函数(`cleanup_module`)的名字是固定的，但是借助于GNU的函数别名机制，可以灵活地指定模块的初始化函数和清除函数的别名
  * **module_init(初始化函数名)**
  * **module_exit(清除函数名)**
* 函数名可以任意指定会带来与内核中的已有的函数名冲突的可能，为了避免因为重名而带来的重复定义的问题，函数可以添加**static**关键字修饰
* 模块的初始化函数会且仅会被调用一次，在调用完成后，该函数不应该再次调用，所以该函数所占用的内存应该被释放掉，在函数名前加**__init**可以达到此目的。`__init`是把标记的函数放到ELF文件的特定代码段，在模块加载这些段时将会单独分配内存，这些函数调用成功后，模块的加载程序会释放这部分内存空间。`__exit`用于修饰清除函数，和`__init`的作用类似，但用于模块的卸载，如果模块不允许卸载，那么这段代码完全就不用加载

#### 内核模块Makefile模板

```makefile
ifeq ($(KERNELRELEASE),)

ifeq ($(ARCH),arm)
KERNELDIR ?= /home/morris/SUDA_V3S/kernel/linux-zero-4.13.y
ROOTSDIR ?= /home/morris/SUDA_V3S/rootfs/multistrap/rootfs
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
endif

PWD ?= $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(ROOTFS) modules_install
clean:
	rm -rf *.o *.ko .*.cmd *.mod.* modules.order Module.symvers .tmp_versions 

else

obj-m := simple.o
simple-objs = first_module.o

endif
```

* KERNELRELEASE是内核源码树中顶层Makefile文件中定义的一个变量，并对其赋值为Linux内核源码的版本，该变量会用export到处，从而可以在子Makefile中使用该变量
* 第一次解释执行该Makefile时，KERNELRELEASE没有被定义。KERNELDIR保存内核源码的路径，ROOTFSDIR保存根文件系统的路径
* Makefile的第一个目标是modules，执行模块编译的任务，仍然是对当前的Makefile使用make命令，其中增加的-C参数用来指定要进入的内核源码的目录，M变量保存内核源码树之外的目录(通常是额外的内核模块源码)。当编译过程折返回编译模块时(退出内核源码目录，再次进入模块目录，由M变量指定)，上述的Makefile将会第二次被解释执行，不过这是KERNELRELEASE变量已经被赋值，并且被导出，导致最开始ifeq条件不成立
* obj-m表示讲后面的目标编译成一个模块
* modules_install目标表示把编译之后的模块安装到指定目录，安装的目录为\$(INSTALL_MOD_PATH)/lib/modules/\$(KERNELRELEASE)，在没有对INSTALL_MOD_PATH赋值的情况下，模块将会被安装到/lib/modules/\$(KERNELRELEASE)目录下
* 上面的例子中，最终生成的模块为simple.ko，它由first_module等目标文件链接而成。这种Makefile格式适用于将一个或者多个源文件编译生成一个内核模块

#### 内核模块相关工具

1. 模块加载
   * insmod，加载指定目录下的.ko文件到内核
   * modprobe，自动加载模块到内核，前提条件是模块要执行安装操作，并且使用depmod命令来更新模块的依赖信息(使用modprobe无需指定路径和后缀)
2. 模块信息
   * modinfo，在安装了模块并运行depmod命令后，可以不指定路径和后缀；也可以指定查看某一特定.ko文件的模块信息
3. 模块卸载
   * rmmod，将指定的模块从内核中卸载
4. 生成模块依赖信息
   * depmod，生成模块的依赖信息，保存在/lib/modules/$(uname -r)/modules.dep文件中

#### 内核模块参数

> 模块参数允许用户在加载模块时通过命令行指定参数值，在模块加载过程中，加载程序会得到命令行参数，并转换成相应类型的值，然后赋值给对应的变量，这个过程发生在调用模块初始化函数之前。
>
> 内核支持的参数类型有：bool，invbool(反转值bool)，charp(字符串指针)，short，int，long，ushort，uint，ulong，这些类型又可以复合成对应的数组类型。

```c
#include <linux/init.h>//包含init_module和cleanup_module的函数原型声明
#include <linux/kernel.h>//包含printk函数原型的声明
#include <linux/module.h>//包含MODULE_LICENSE等宏定义

static int baudrate = 9600;
static int port[4] = {0,1,2,3};
static int port_num = sizeof(port)/sizeof(port[0]);
static char* name = "vser";

/* module_param(name,type,perm) */
module_param(baudrate,int,S_IRUGO);
module_param(name,charp,S_IRUGO);
/* module_param_array(name,type,nump,perm) */
module_param_array(port,int,&port_num,S_IRUGO);

/* 模块初始化函数 */
static int __init vser_init(void){
    int i;
    printk("vser_init");
    printk("baudrate:%d",baudrate);
    printk("port:");
    for(i=0;i<port_num;i++){
        printk("%d ",port[i]);
    }
    printk("name:%s",name);

    return 0;
}

/* 模块清理函数 */
static void __exit vser_exit(void){
    printk("vser_exit");
}

/* 为模块初始化函数和清理函数取别名 */
module_init(vser_init);
module_exit(vser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("first_module");
```

* 模块加载到内核后，可以在sysfs文件系统中发现和模块参数对应的文件以及相应的权限`ls /sys/module/simple/parameters`

#### 内核模块依赖

> .ko文件是一个普通的ELF目标文件，使用nm命令查看模块目标文件的符号信息`nm simple.ko`
>
> ```shell
> 0000000000000020 d baudrate
> 0000000000000000 T cleanup_module
> 0000000000000000 T init_module
> 00000000000000d0 r __module_depends
> 0000000000000000 d name
>                  U param_array_ops
> 0000000000000020 r __param_arr_port
> 0000000000000050 r __param_baudrate
> 0000000000000028 r __param_name
>                  U param_ops_charp
>                  U param_ops_int
> 0000000000000000 r __param_port
> 0000000000000048 r __param_str_baudrate
> 0000000000000040 r __param_str_name
> 0000000000000000 r __param_str_port
> 0000000000000010 d port
> 0000000000000008 d port_num
>                  U printk
> 0000000000000000 D __this_module
> 0000000000000000 r __UNIQUE_ID_alias14
> 000000000000002f r __UNIQUE_ID_author12
> 0000000000000090 r __UNIQUE_ID_baudratetype8
> 0000000000000013 r __UNIQUE_ID_description13
> 0000000000000055 r __UNIQUE_ID_license11
> 00000000000000d9 r __UNIQUE_ID_name9
> 000000000000007c r __UNIQUE_ID_nametype9
> 0000000000000061 r __UNIQUE_ID_porttype10
> 00000000000000a8 r __UNIQUE_ID_srcversion10
> 00000000000000e5 r __UNIQUE_ID_vermagic8
> 0000000000000000 t vser_exit
> 0000000000000000 t vser_init
> ```
>
> vser_exit和vser_init的符号类型是t，表示它们是函数；而printk的符号类型是U，表示它们是一个未决符号，在编译阶段无法知道这个符号的地址，因为它被定义在其他文件中。
>
> 在printk的实现代码中(kernel/printk/printk.c)中，通过`EXPORT_SYMBOL(printkk)`宏将printk导出，其目的是为动态加载的模块提供printk的地址信息。
>
> **注意**：两个模块存在依赖关系，如果分别单独编译两个模块，这两个模块将无法加载成功。解决办法是将两个模块放在一起编译，或者将被依赖的模块集成到内核源码中编译。



### 设备驱动分类

1. 字符设备驱动

   > 设备对数据的处理是按照字节流的形式进行的，可以支持随机访问(比如帧缓存设备)，也可以不支持随机访问(比如串口)，因为数据流量通常不是很大，所以一般没有页高速缓存。

2. 块设备驱动

   > 设备对数据的处理是按照若干个块进行的，一个块有其固定的大小，比如硬盘的一个扇区通常是512字节，那么每次读写的数据至少就是512字节。这类设备通常都支持随机访问，并且为了提高效率，可以将之前用到的数据缓存起来，以便下次使用。

3. 网络设备驱动

   > 它是专门针对网络设备的一类驱动，其主要作用是进行网络数据的收发。



### 字符设备驱动基础

1. mknod命令

   > 在现在的Linux系统中，设备文件是自动创建的，即便如此，我们还是可以通过mknod命令来手动创建一个设备文件。mknod命令将文件名、文件类型和主次设备号等信息保存在了磁盘上。

2. 如何打开一个文件

   ![文件操作的相关数据结构及关系](imgs/file-operations-related-structs.png)

   > 在内核中，一个进程用一个task_struct结构对象来表示，其中的files成员指向了一个files_struct结构变量，该结构体中有一个fd_array的指针数组(用于维护打开文件的信息)，数组的每一个元素是指向file结构的一个指针。open系统调用函数在内核中对应的函数是sys_open，sys_open调用了do_sys_open，在do_sys_open中首先调用getname函数将文件名从用户空间复制到内核空间。接着调用get_unused_fd_flags来获取一个未使用的文件描述符，要获得该描述符，其实就是搜索files_struct中的fd_array数组，查看哪一个元素没有被使用，然后**返回其下标**即可。接下来调用do_filp_open函数来构造一个file结构，并初始化里面的成员。其中最重要的是将它的f_op成员指向和设备对应的驱动程序的操作方法集合的结构file_operations，这个结构中的绝大多数成员都是函数指针，通过file_operations中的open函数指针可以调用驱动中实现的特定于设备的打开函数，从而完成打开的操作。do_filp_open函数执行成功后，调用fd_install函数，该函数将刚才得到的文件描述符作为访问fd_array数组的下标，让下标对应的元素指向新构造的file结构。最后系统调用返回到应用层，将刚才的数组下标作为打开文件的文件描述符返回。 
   >
   > do_filp_open函数是这个过程中最复杂的一部分，简单来说，如果用户是第一次打开一个文件，那么会从磁盘中获取之前使用mknod保存的节点信息(文件类型，设备号等)，将其保存到内存中的inode结构体中。另外，通过判断文件的类型，还将inode中的f_op指针指向了def_chr_fops，这个结构体中的open函数指向了chrdev_open，其完成的主要工作是：首先根据设备号找到添加在内核中代表字符设备的cdev(cdev是放在cdev_map散列表中的，驱动加载时会构造相应的cdev并添加到这个散列表中，并且在构造这个cdev时还实现了一个操作方法集合，由cdev的ops成员指向它)，找到对应的cdev后，用cdev关联的操作方法集合替代之前构造的file结构体中的操作方法集合，然后调用cdev所关联的操作方法集合中的打开函数，完成设备真正的打开函数。
   >
   > 为了下一次能够快速打开文件，内核在第一次打开一个文件或目录时都会创建一个dentry的目录项，它保存了文件名和所对应的inode信息，所有的dentry使用散列的方式存储在目录项高速缓存中，内核在打开文件时会先在这个高速缓存中查找相应的dentry，如果找到，则可以立即获取文件所对应的inode，否则就会在磁盘上获取。
   >
   > 可以说，**字符设备驱动的框架就是围绕着设备号、cdev和操作方法集合来实现的。**

3. 查看内核中已经安装的设备驱动:`cat /proc/devices`



### 字符设备驱动框架

> 要实现一个字符设备驱动，最重要的事就是**要构造一个cdev结构对象，并让cdev同设备号和设备的操作方法集合相关联，然后将该cdev结构对象添加到内核的cdev_mao散列表中**

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>

#define VSER_MAJOR      256
#define VSER_MINOR      0
#define VSER_DEV_CNT    1
#define VSER_DEV_NAME   "vser"

static struct cdev vsdev;
DEFINE_KFIFO(vsfifo,char,32);/* 定义并初始化一个内核FIFO，元素个数必须是2的幂 */

static int vser_open(struct inode* inode,struct file* filp){
    return 0;
}

static int vser_release(struct inode* inode,struct file* filp){
    return 0;
}

static ssize_t vser_read(struct file* filp,char __user* buf,size_t count,loff_t* pos){
    unsigned int copied = 0;
    /* 将内核FIFO中的数据取出，复制到用户空间 */
    int ret = kfifo_to_user(&vsfifo,buf,count,&copied);
    if(ret){
        return ret;
    }
    return copied;
}

static ssize_t vser_write(struct file* filp,const char __user* buf,size_t count,loff_t* pos){
    unsigned int copied = 0;
    /* 将用户空间的数据放入内核FIFO中 */
    int ret = kfifo_from_user(&vsfifo,buf,count,&copied);
    if(ret){
        return ret;
    }
    return copied;
}

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
};

/* 模块初始化函数 */
static int __init vser_init(void){
    int ret;
    dev_t dev;

    dev = MKDEV(VSER_MAJOR,VSER_MINOR);
    /* 向内核注册设备号，静态方式 */
    ret = register_chrdev_region(dev,VSER_DEV_CNT,VSER_DEV_NAME);
    if(ret){
        goto reg_err;
    }
    /* 初始化cdev对象，绑定ops */
    cdev_init(&vsdev,&vser_ops);
    vsdev.owner = THIS_MODULE;
    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&vsdev,dev,VSER_DEV_CNT);
    if(ret){
        goto add_err;
    }
    return 0;

add_err:
    unregister_chrdev_region(dev,VSER_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit vser_exit(void){
    dev_t dev;
    dev = MKDEV(VSER_MAJOR,VSER_MINOR);
    cdev_del(&vsdev);
    unregister_chrdev_region(dev,VSER_DEV_CNT);
}

/* 为模块初始化函数和清理函数取别名 */
module_init(vser_init);
module_exit(vser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("virtual-serial");
```

* **THIS_MODULE**是包含驱动的模块中的struct module类型对象的地址，类似于C++中的this指针，这样就能通过cdev或者fops找到对应的模块，在对前面两个对象访问时都要调用类似于try_module_get的函数增加模块的引用计数，因为在这两个对象使用的过程中，模块是不能被卸载的，模块被卸载的前提条件是引用计数为0



### 一个驱动支持多个设备

> 1. 首先要向内核注册多个设备号
> 2. 其次就是在添加cdev对象的时候指明该cdev对象管理了多个设备，或者添加多个cdev对象，每个cdev对象管理一个设备
> 3. 最麻烦的在于读写操作的时候区分对哪个设备进行操作。open接口中的inode形参中包含了对应设备的设备号以及所对应的cdev对象的地址。因此可以在open接口函数中读取这些信息，并存放在file结构体对象的某个成员中，再在读写的接口函数中获取该file结构的成员，从而可以区分出对哪个设备进行操作

#### 使用一个cdev实现对多个设备的支持

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>

#define VSER_MAJOR      256
#define VSER_MINOR      0
#define VSER_DEV_CNT    2   //本驱动支持两个设备
#define VSER_DEV_NAME   "vser"

static struct cdev vsdev;
DEFINE_KFIFO(vsfifo0,char,32);
DEFINE_KFIFO(vsfifo1,char,32);

static int vser_open(struct inode* inode,struct file* filp){
    /* 根据次设备号来区分具体的设备 */
    switch(MINOR(inode->i_rdev)){
        default:
        case 0:
            filp->private_data = &vsfifo0;
            break;
        case 1:
            filp->private_data = &vsfifo1;
            break;
    }
    return 0;
}

static int vser_release(struct inode* inode,struct file* filp){
    return 0;
}

static ssize_t vser_read(struct file* filp,char __user* buf,size_t count,loff_t* pos){
    unsigned int copied = 0;
    /* 将内核FIFO中的数据取出，复制到用户空间 */
    struct kfifo* vsfifo = filp->private_data;
    int ret = kfifo_to_user(vsfifo,buf,count,&copied);
    if(ret){
        return ret;
    }
    return copied;
}

static ssize_t vser_write(struct file* filp,const char __user* buf,size_t count,loff_t* pos){
    unsigned int copied = 0;
    /* 将用户空间的数据放入内核FIFO中 */
    struct kfifo* vsfifo = filp->private_data;
    int ret = kfifo_from_user(vsfifo,buf,count,&copied);
    if(ret){
        return ret;
    }
    return copied;
}

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
};

/* 模块初始化函数 */
static int __init vser_init(void){
    int ret;
    dev_t dev;

    dev = MKDEV(VSER_MAJOR,VSER_MINOR);
    /* 向内核注册设备号，静态方式 */
    ret = register_chrdev_region(dev,VSER_DEV_CNT,VSER_DEV_NAME);
    if(ret){
        goto reg_err;
    }
    /* 初始化cdev对象，绑定ops */
    cdev_init(&vsdev,&vser_ops);
    vsdev.owner = THIS_MODULE;
    /* 添加到内核中的cdev_map散列表中 */
    ret = cdev_add(&vsdev,dev,VSER_DEV_CNT);
    if(ret){
        goto add_err;
    }
    return 0;

add_err:
    unregister_chrdev_region(dev,VSER_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit vser_exit(void){
    dev_t dev;
    dev = MKDEV(VSER_MAJOR,VSER_MINOR);
    cdev_del(&vsdev);
    unregister_chrdev_region(dev,VSER_DEV_CNT);
}

/* 为模块初始化函数和清理函数取别名 */
module_init(vser_init);
module_exit(vser_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple module");
MODULE_ALIAS("virtual-serial");
```

#### 将每一个cdev对象对应到一个设备

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>

#define VSER_MAJOR 256
#define VSER_MINOR 0
#define VSER_DEV_CNT 2 //本驱动支持两个设备
#define VSER_DEV_NAME "vser"

static DEFINE_KFIFO(vsfifo0, char, 32);
static DEFINE_KFIFO(vsfifo1, char, 32);

struct vser_dev
{
    struct cdev cdev;
    struct kfifo *fifo;
};

static struct vser_dev vsdev[2];

static int vser_open(struct inode *inode, struct file *filp)
{
    struct vser_dev *vsdev = container_of(inode->i_cdev, struct vser_dev, cdev);
    filp->private_data = vsdev->fifo;
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
    struct kfifo *vsfifo = filp->private_data;
    int ret = kfifo_to_user(vsfifo, buf, count, &copied);
    if (ret)
    {
        return ret;
    }
    return copied;
}

static ssize_t vser_write(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
    unsigned int copied = 0;
    /* 将用户空间的数据放入内核FIFO中 */
    struct kfifo *vsfifo = filp->private_data;
    int ret = kfifo_from_user(vsfifo, buf, count, &copied);
    if (ret)
    {
        return ret;
    }
    return copied;
}

static struct file_operations vser_ops = {
    .owner = THIS_MODULE,
    .open = vser_open,
    .release = vser_release,
    .read = vser_read,
    .write = vser_write,
};

/* 模块初始化函数 */
static int __init vser_init(void)
{
    int ret;
    dev_t dev;
    int i;

    dev = MKDEV(VSER_MAJOR, VSER_MINOR);
    /* 向内核注册设备号，静态方式 */
    ret = register_chrdev_region(dev, VSER_DEV_CNT, VSER_DEV_NAME);
    if (ret)
    {
        goto reg_err;
    }
    /* 初始化cdev对象，绑定ops */
    for (i = 0; i < VSER_DEV_CNT; i++)
    {
        cdev_init(&vsdev[i].cdev, &vser_ops);
        vsdev[i].cdev.owner = THIS_MODULE;
        vsdev[i].fifo = i == 0 ? (struct kfifo *)&vsfifo0 : (struct kfifo *)&vsfifo1;
        /* 添加到内核中的cdev_map散列表中 */
        ret = cdev_add(&vsdev[i].cdev, dev + i, 1);
        if (ret)
        {
            goto add_err;
        }
    }
    return 0;

add_err:
    for (--i; i > 0; --i)
    {
        cdev_del(&vsdev[i].cdev);
    }
    unregister_chrdev_region(dev, VSER_DEV_CNT);
reg_err:
    return ret;
}

/* 模块清理函数 */
static void __exit vser_exit(void)
{
    dev_t dev;
    int i;

    dev = MKDEV(VSER_MAJOR, VSER_MINOR);
    for (i = 0; i < VSER_DEV_CNT; i++)
    {
        cdev_del(&vsdev[i].cdev);
    }
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



