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