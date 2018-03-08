# About Device Model
> 在Linux系统中有一个sysfs伪文件系统，挂载于/sys目录下，该目录详细罗列了所有与设备、驱动和硬件相关的信息。**当向内核成功添加一个kobject对象后，底层的代码会自动在/sys目录下生成一个子目录。**另外，kobject可以附加一些属性，并绑定操作这些属性的方法，当向内核成功添加一个kobject对象后，其附加的属性会被底层的代码自动实现为对象对应目录下的文件，用户访问这些文件最终就变成了调用操作属性的方法来访问其属性。最后，通过sys的API接口可以将两个kobject对象关联起来，形成软链接。
>
> 除了struct kobject，还有一个叫struct set的类，它是多个kobject对象的集合，也就是多个kobject对象可以通过一个kset集合在一起。kset本身也内嵌了一个kobject，它可以作为集合中的kobject对象的父对象，从而在kobject之间形成父子关系，这种父子关系在/sys目录下体现为父目录和子目录的关系。而属于同一集合的kobject对象形成兄弟关系，在/sys目录下体现为同级目录。kset也可以附加属性，从而在对应的目录下产生文件。

####kset与kobject的测试

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/slab.h>
#include <linux/kobject.h>

static struct kset *kset;
static struct kobject *kobj1;
static struct kobject *kobj2;
static unsigned int val = 0;

static ssize_t val_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t val_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *endp;

	printk("size = %d\n", count);
	val = simple_strtoul(buf, &endp, 10);

	return count;
}

static struct kobj_attribute kobj1_val_attr = __ATTR(val, 0664, val_show, val_store);
static struct attribute *kobj1_attrs[] = {
	&kobj1_val_attr.attr,
	NULL,
};

static struct attribute_group kobj1_attr_group = {
	.attrs = kobj1_attrs,
};

static int __init model_init(void)
{
	int ret;
	/* 创建kset对象，没有指定父对象，所以kset目录将会被创建在/sys下 */
	kset = kset_create_and_add("kset", NULL, NULL);
	/* kobj1和kobj2位于kset目录之下 */
	kobj1 = kobject_create_and_add("kobj1", &kset->kobj);
	kobj2 = kobject_create_and_add("kobj2", &kset->kobj);
	/* kobj1附加了一个属性，在文件系统中表现为kobj1目录下的一个文件 */
	ret = sysfs_create_group(kobj1, &kobj1_attr_group);
	/* 在kobj2下创建了一个软链接kobj1 */
	ret = sysfs_create_link(kobj2, kobj1, "kobj1");

	return 0;
}

static void __exit model_exit(void)
{
	sysfs_remove_link(kobj2, "kobj1");
	sysfs_remove_group(kobj1, &kobj1_attr_group);
	kobject_del(kobj2);
	kobject_del(kobj1);
	kset_unregister(kset);
}

module_init(model_init);
module_exit(model_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("A simple module for device model");      
```



### 总线、设备和驱动

> 在Linux设备模型中，struct bus_type代表总线，struct device代表设备，struct device_driver代表驱动。这三者都内嵌了struct kobject或者struct kset，于是就会生成对应的总线、设备和驱动的目录。另外，Linux内核还为这些kobject和kset对象附加了很多属性，于是也产生了很多对应目录下的文件。
>
> 设备专门用来描述设备所占用的资源信息，而驱动和设备绑定成功后，驱动负责从设备中动态获取这些资源信息，当设备的资源改变后，只是设备改变了而已，驱动的代码可以不做任何修改，这就大大提高了驱动代码的通用性。

* vbus驱动

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/device.h>

/* 用于匹配驱动和设备 */
static int vbus_match(struct device *dev, struct device_driver *drv)
{
	/* 一般会考察设备和驱动的ID是否匹配 */
	return 1;
}
/* 定义了一个vbus对象，代表总线 */
static struct bus_type vbus = {
	.name = "vbus",
	.match = vbus_match,
};
/* 导出vbus对象，其余模块也能访问 */
EXPORT_SYMBOL(vbus);

static int __init vbus_init(void)
{
	/* 向内核注册总线 */
	return bus_register(&vbus);
}

static void __exit vbus_exit(void)
{
	/* 总线注销 */
	bus_unregister(&vbus);
}

module_init(vbus_init);
module_exit(vbus_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A virtual bus");
```

* vdrv驱动

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/device.h>

/* 引用了vbus模块导出的符号vbus */
extern struct bus_type vbus;

/* 定义了vdrv对象，代表驱动 */
static struct device_driver vdrv = {
	.name = "vdrv",
	.bus = &vbus, //指明该驱动所属的总线，这样该驱动就会被注册在vbus总线下
};

static int __init vdrv_init(void)
{
	/* 注册驱动 */
	return driver_register(&vdrv);
}

static void __exit vdrv_exit(void)
{
	/* 注销驱动 */
	driver_unregister(&vdrv);
}

module_init(vdrv_init);
module_exit(vdrv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A virtual device driver");
```

* vdev驱动

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/device.h>

extern struct bus_type vbus;

static void vdev_release(struct device *dev)
{
}

/* 定义vdev对象，代表设备 */
static struct device vdev = {
	.init_name = "vdev",
	.bus = &vbus, //指定设备所属的总线，这样注册的时候就会挂在vbus总线下
	.release = vdev_release,
};

static int __init vdev_init(void)
{
	/* 注册设备 */
	return device_register(&vdev);
}

static void __exit vdev_exit(void)
{
	/* 注销设备 */
	device_unregister(&vdev);
}

module_init(vdev_init);
module_exit(vdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A virtual device");
```



### 平台设备及其驱动

> 要满足Linux设备模型，就必须要有总线、设备和驱动。但是有的设备并没有对应的物理总线，比如LED、RTC和蜂鸣器等，为此，内核专门开发了一种虚拟总线——platform总线，用来连接这些没有物理总线的设备或者一些不支持热插拔的设备。

* 平台设备

```c
struct platform_device{
  const char *name;
  int id;//用于区别同类型的不同平台设备
  bool id_auto;
  struct device dev;
  u32 num_resources;
  struct resource *resource;//平台设备的资源列表(数组)
  const struct platform_device_id *id_entry;//用于同平台驱动匹配的ID，在平台总线的match函数中首先尝试匹配该ID，如果不成功再尝试用name来匹配
  struct mfd_cell *mfd_cell;
  struct pdev_archdata archdata;
};
```

> 在平台设备中，最关键的就是设备使用的资源信息的描述，这是实现设备和驱动分离的关键。

```c
struct resource{
  resource_size_t start;//资源的开始，对于IO内存来说就是起始的内存地址，对于中断资源来说就是起始的中断号，对于DMA资源来说就是起始的DMA通道号
  resource_size_t end;//资源的结束
  const char* name;
  unsigned long flags;//资源的标志，IORESOURCE_MEM,IORESOURCE_IRQ,IORESOURCE_DMA
  struct resource* parent,*sibling,*child;
};
```

> 平台设备及其资源通常存在于BSP文件中，例如arch/arm/mach-s3c24xx/mach-qt2410.c

```c
static struct resource qt2410_cs89x0_resources[] = {
  [0] = DEFINE_RES_MEM(0x19000000,17),
  [1] = DEFINE_RES_IRQ(IRQ_EINT9),
};
static struct platform_device qt2410_cs89x0 = {
  .name = "cirrus-cs89x0",
  .num_resources = ARRAY_SIZE(qt2410_cs89x0_resources),
  .resource = qt2410_cs89x0_resources,
};
```

> 向平台总线注册和注销平台设备的主要函数

```c
int platform_add_devices(struct platform_device **devs,int num);//一次注册多个平台设备
int platform_device_register(struct platform_device *pdev);//一次只注册一个平台设备
void platform_device_unregister(struct platform_device *pdev);//注销平台设备
```

> 当平台总线发现有和平台设备匹配的驱动时，就会调用平台驱动内的一个函数，并传递匹配的平台设备结构地址，平台驱动就可以从中获取设备的资源信息。

```c
struct resource *platform_get_resource(struct platform_device *dev,unsigned int type,unsigned int num);//从平台设备dev中获取类型为type，序号为num的资源
resource_size_t resource_size(const struct resource *res);//返回资源的大小
```



* 平台驱动

```c
struct platform_driver{
  int (*probe)(struct platform_device*);//总线发现有匹配的平台设备时调用
  int (*remove)(struct platform_device*);//所驱动的平台设备被移除时或平台驱动注销时调用
  void (*shutdown)(struct platform_device*);//设备掉电时被调用
  int (*suspend)(struct platform_device*,pm_message_t state);//设备挂起时被调用
  int (*resume)(struct platform_device*);//设备恢复时被调用
  struct device_driver driver;
  const struct platform_device_id *id_table;//平台驱动可以驱动的平台设备ID列表，可用于和平台设备匹配
  bool prevent_deferred_probe;
}
```

> 向平台总线注册和注销平台驱动的函数有

```c
platform_driver_register(drv);
void plstform_driver_unregister(struct platform_driver*);
```

#### 平台驱动简单实例

* 平台设备

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/platform_device.h>

static void pdev_release(struct device *dev)
{
}

/* 定义平台设备pdev0 */
struct platform_device pdev0 = {
	.name = "pdev",
	.id = 0,
	.num_resources = 0,
	.resource = NULL,
	.dev = {
		.release = pdev_release,
	},
};

/* 定义平台设备pdev1 */
struct platform_device pdev1 = {
	.name = "pdev",
	.id = 1,
	.num_resources = 0,
	.resource = NULL,
	.dev = {
		.release = pdev_release,
	},
};

static int __init pltdev_init(void)
{
	/* 注册平台设备 */
	platform_device_register(&pdev0);
	platform_device_register(&pdev1);

	return 0;
}

static void __exit pltdev_exit(void)
{
	/* 注销平台设备 */
	platform_device_unregister(&pdev1);
	platform_device_unregister(&pdev0);
}

module_init(pltdev_init);
module_exit(pltdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("register a platfom device");
```

* 平台驱动

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/platform_device.h>

static int pdrv_suspend(struct device *dev)
{
	printk("pdev: suspend\n");
	return 0;
}

static int pdrv_resume(struct device *dev)
{
	printk("pdev: resume\n");
	return 0;
}

/* 电源管理函数的集合 */
static const struct dev_pm_ops pdrv_pm_ops = {
	.suspend = pdrv_suspend,
	.resume  = pdrv_resume,
};

static int pdrv_probe(struct platform_device *pdev)
{
	return 0;
}

static int pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

/* 定义一个平台驱动 */
struct platform_driver pdrv = {
	.driver = {
		.name    = "pdev",//名字需要和待绑定的平台设备一致，这样才能匹配成功
		.owner   = THIS_MODULE,
		.pm      = &pdrv_pm_ops,
	},
	.probe   = pdrv_probe,
	.remove  = pdrv_remove,
};

static int __init pltdrv_init(void)
{
	/* 注册平台驱动 */
	platform_driver_register(&pdrv);
	return 0;
}

static void __exit pltdrv_exit(void)
{
	/* 注销平台驱动 */
	platform_driver_unregister(&pdrv);
}

module_init(pltdrv_init);
module_exit(pltdrv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("suda-morris <362953310@qq.com>");
MODULE_DESCRIPTION("A simple platform driver");
MODULE_ALIAS("platform:pdev");
```



### udev与驱动的自动加载

> 使用了Linux设备模型后，任何设备的添加、删除或状态修改都会导致内核向用户空间发送相应的事件，这个事件叫uevent，和kobject密切关联。这样用户空间就可以捕获这些事件来自动完成某些操作，比如自动加载驱动、自动创建和删除设备节点、修改权限、创建软链接、修改网络设备名等。目前实现这个功能的工具就是udev(嵌入式设备下通常使用mdev)。这是一个用户空间的应用程序，捕获来自内核空间的事件，然后分局其规则文件进行操作。udev的规则文件为/etc/udev/rules.d目录下后缀为.rules的文件。

* udev的规则

> udev规则文件用#来注释，除此以外的就是一条一条的规则。每条规则至少包含一个键值对，键分为**匹配**和**赋值**两种类型。如果内核发来的事件匹配了规则中的所有匹配键的值，那么这条规则就可以得到应用，并且赋值键被赋予指定的值。一条规则包含了一个或多个键值对，这些键值对用逗号隔开，每个键由操作符规定一个操作。

* 合法的操作符

```bash
==和!=：判等，用于匹配键
=、+=和:=：赋值，用于赋值键
```

* 常见的键如下

```bash
ACTION：事件动作的名字，如add表示添加
DEVPATH：事件设备的路径
KERNEL：事件设备的名字
NAME：节点或网络接口的名字
SUBSYSTEWM：事件设备子系统
DRIVER：事件设备驱动的名字
ENV{key}：设备的属性
OWNER、GROUP、MODE：设备节点的权限
RUN：添加一个和设备相关的命令到命令列表中
IMPORT{type}：导入一组设备属性的变量，依赖于类型type
```

* ```在Ubuntu中**自动加载驱动**的规则如下，将这条规则添加到/etc/udev/rules.d/40-modprobe.rules文件中：

  ```

```bash
#根据模块的别名信息，用modprobe命令加载对应的内核模块。`
ENV{MODALIAS}=="?*",RUN+="/sbin/modprobe $env{MODALIAS}"
```



#### 使用平台设备的LED驱动

* 平台设备

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/platform_device.h>

static void fsdev_release(struct device *dev)
{
}

static struct resource led2_resources[] = {
	[0] = DEFINE_RES_MEM(0x11000C40, 4),
};

static struct resource led3_resources[] = {
	[0] = DEFINE_RES_MEM(0x11000C20, 4),
};

static struct resource led4_resources[] = {
	[0] = DEFINE_RES_MEM(0x114001E0, 4),
};

static struct resource led5_resources[] = {
	[0] = DEFINE_RES_MEM(0x114001E0, 4),
};

unsigned int led2pin = 7;
unsigned int led3pin = 0;
unsigned int led4pin = 4;
unsigned int led5pin = 5;

struct platform_device fsled2 = {
	.name = "fsled",
	.id = 2,
	.num_resources = ARRAY_SIZE(led2_resources),
	.resource = led2_resources,
	.dev = {
		.release = fsdev_release,
		.platform_data = &led2pin, //用来向驱动传递更多的信息
	},
};

struct platform_device fsled3 = {
	.name = "fsled",
	.id = 3,
	.num_resources = ARRAY_SIZE(led3_resources),
	.resource = led3_resources,
	.dev = {
		.release = fsdev_release,
		.platform_data = &led3pin,
	},
};

struct platform_device fsled4 = {
	.name = "fsled",
	.id = 4,
	.num_resources = ARRAY_SIZE(led4_resources),
	.resource = led4_resources,
	.dev = {
		.release = fsdev_release,
		.platform_data = &led4pin,
	},
};

struct platform_device fsled5 = {
	.name = "fsled",
	.id = 5,
	.num_resources = ARRAY_SIZE(led5_resources),
	.resource = led5_resources,
	.dev = {
		.release = fsdev_release,
		.platform_data = &led5pin,
	},
};

static struct platform_device *fsled_devices[] = {
	&fsled2,
	&fsled3,
	&fsled4,
	&fsled5,
};

static int __init fsdev_init(void)
{
	return platform_add_devices(fsled_devices, ARRAY_SIZE(fsled_devices));
}

static void __exit fsdev_exit(void)
{
	platform_device_unregister(&fsled5);
	platform_device_unregister(&fsled4);
	platform_device_unregister(&fsled3);
	platform_device_unregister(&fsled2);
}

module_init(fsdev_init);
module_exit(fsdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("register LED devices");
```

* 平台驱动

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>

#include "fsled.h"

#define FSLED_MAJOR 256
#define FSLED_DEV_NAME "fsled"

struct fsled_dev
{
	unsigned int __iomem *con;
	unsigned int __iomem *dat;
	unsigned int pin;
	atomic_t available;
	struct cdev cdev;
};

static int fsled_open(struct inode *inode, struct file *filp)
{
	struct fsled_dev *fsled = container_of(inode->i_cdev, struct fsled_dev, cdev);

	filp->private_data = fsled;
	if (atomic_dec_and_test(&fsled->available))
		return 0;
	else
	{
		atomic_inc(&fsled->available);
		return -EBUSY;
	}
}

static int fsled_release(struct inode *inode, struct file *filp)
{
	struct fsled_dev *fsled = filp->private_data;

	writel(readl(fsled->dat) & ~(0x1 << fsled->pin), fsled->dat);

	atomic_inc(&fsled->available);
	return 0;
}

static long fsled_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct fsled_dev *fsled = filp->private_data;

	if (_IOC_TYPE(cmd) != FSLED_MAGIC)
		return -ENOTTY;

	switch (cmd)
	{
	case FSLED_ON:
		writel(readl(fsled->dat) | (0x1 << fsled->pin), fsled->dat);
		break;
	case FSLED_OFF:
		writel(readl(fsled->dat) & ~(0x1 << fsled->pin), fsled->dat);
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static struct file_operations fsled_ops = {
	.owner = THIS_MODULE,
	.open = fsled_open,
	.release = fsled_release,
	.unlocked_ioctl = fsled_ioctl,
};

static int fsled_probe(struct platform_device *pdev)
{
	int ret;
	dev_t dev;
	struct fsled_dev *fsled;
	struct resource *res;
	unsigned int pin = *(unsigned int *)pdev->dev.platform_data;

	dev = MKDEV(FSLED_MAJOR, pdev->id);
	ret = register_chrdev_region(dev, 1, FSLED_DEV_NAME);
	if (ret)
		goto reg_err;

	fsled = kzalloc(sizeof(struct fsled_dev), GFP_KERNEL);
	if (!fsled)
	{
		ret = -ENOMEM;
		goto mem_err;
	}

	cdev_init(&fsled->cdev, &fsled_ops);
	fsled->cdev.owner = THIS_MODULE;
	ret = cdev_add(&fsled->cdev, dev, 1);
	if (ret)
		goto add_err;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0); //获取IO内存资源信息
	if (!res)
	{
		ret = -ENOENT;
		goto res_err;
	}

	fsled->con = ioremap(res->start, resource_size(res));
	if (!fsled->con)
	{
		ret = -EBUSY;
		goto map_err;
	}
	fsled->dat = fsled->con + 1;

	fsled->pin = pin;
	atomic_set(&fsled->available, 1);
	writel((readl(fsled->con) & ~(0xF << 4 * fsled->pin)) | (0x1 << 4 * fsled->pin), fsled->con);
	writel(readl(fsled->dat) & ~(0x1 << fsled->pin), fsled->dat);
	platform_set_drvdata(pdev, fsled); //将动态分配得到的fsled保存到平台设备中

	return 0;

map_err:
res_err:
	cdev_del(&fsled->cdev);
add_err:
	kfree(fsled);
mem_err:
	unregister_chrdev_region(dev, 1);
reg_err:
	return ret;
}

static int fsled_remove(struct platform_device *pdev)
{
	dev_t dev;
	struct fsled_dev *fsled = platform_get_drvdata(pdev);

	dev = MKDEV(FSLED_MAJOR, pdev->id);

	iounmap(fsled->con);
	cdev_del(&fsled->cdev);
	kfree(fsled);
	unregister_chrdev_region(dev, 1);

	return 0;
}

struct platform_driver fsled_drv = {
	.driver = {
		.name = "fsled", //名字要和平台设备匹配
		.owner = THIS_MODULE,
	},
	.probe = fsled_probe,
	.remove = fsled_remove,
};

module_platform_driver(fsled_drv);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("A simple character device driver for LEDs on FS4412 board");
```

* 应用层测试代码

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include "fsled.h"

int main(int argc, char *argv[])
{
	int fd[4];
	int ret;
	int num = 0;

	fd[0] = open("/dev/led2", O_RDWR);
	if (fd[0] == -1)
		goto fail;
	fd[1] = open("/dev/led3", O_RDWR);
	if (fd[1] == -1)
		goto fail;
	fd[2] = open("/dev/led4", O_RDWR);
	if (fd[2] == -1)
		goto fail;
	fd[3] = open("/dev/led5", O_RDWR);
	if (fd[3] == -1)
		goto fail;

	while (1)
	{
		ret = ioctl(fd[num], FSLED_ON);
		if (ret == -1)
			goto fail;
		usleep(500000);
		ret = ioctl(fd[num], FSLED_OFF);
		if (ret == -1)
			goto fail;
		usleep(500000);

		num = (num + 1) % 4;
	}
fail:
	perror("led test");
	exit(EXIT_FAILURE);
}
```



### 自动创建设备节点

> 内核中设备的添加、删除或修改都会向应用层发送热插拔事件，应用程序可以捕获到这些事件来自动完成某些操作，如自动加载驱动、自动创建设备节点等。这里以mdev为例来说明自动创建设备节点的两种方法。

* mdev -s

> 通常在根文件系统挂载完成后，运行mdev -s命令，它将递归扫描**/sys/block**目录和**/sys/class**目录下的文件，根据文件的内容来自动创建设备文件。

* 实时捕获热插拔事件

> 当内核发生了热插拔事件后，mdev会自动被调用，这体现在根文件系统的初始化脚本文件中的：
>
> `echo /sbin/mdev > /proc/sys/kernel/hotplug`
>
> 内核有一种在发生热插拔事件后调用应用程序的方式，那就是执行/proc/sys/kernel/hotplug文件中的程序，这种方式比较简单，所以常在嵌入式系统之中使用。发生热插拔事件时，调用mdev程序会将热插拔信息放在环境变量和参数当中，mdev程序利用这些信息就可以自动创建设备节点。

#### 在代码中为udev自动创建设备节点提供必要信息

> 自动设备节点的创建要依赖热插拔事件和sysfs文件系统，mdev扫描/sys/class目录暗示我们要创建类，并且在类下面应该有具体的设备。

```c
struct class* class_create(owner,name);//创建类，owner是所属的模块对象指针，name是类的名字
void class_destroy(struct class* cls);//销毁cls类
struct device *device_create(struct class* class,struct device *parent,dev_t devt,void* drvdata,const char* fmt,...);//在类class下创建设备，parent是父设备，没有则为NULL，devt是设备号，drvdata是驱动数据，没有则为NULL，fmt是格式化字符串
void device_destroy(struct class* cls,dev_t devt);//销毁cls类下面主次设备号为devt的设备。
```

* 驱动范例

```c
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>

#include "fsled.h"

#define FSLED_MAJOR 256
#define FSLED_DEV_NAME "fsled"

struct fsled_dev
{
	unsigned int __iomem *con;
	unsigned int __iomem *dat;
	unsigned int pin;
	atomic_t available;
	struct cdev cdev;
	struct device *dev;
};

struct class *fsled_cls;

static int fsled_open(struct inode *inode, struct file *filp)
{
	struct fsled_dev *fsled = container_of(inode->i_cdev, struct fsled_dev, cdev);

	filp->private_data = fsled;
	if (atomic_dec_and_test(&fsled->available))
		return 0;
	else
	{
		atomic_inc(&fsled->available);
		return -EBUSY;
	}
}

static int fsled_release(struct inode *inode, struct file *filp)
{
	struct fsled_dev *fsled = filp->private_data;

	writel(readl(fsled->dat) & ~(0x1 << fsled->pin), fsled->dat);

	atomic_inc(&fsled->available);
	return 0;
}

static long fsled_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct fsled_dev *fsled = filp->private_data;

	if (_IOC_TYPE(cmd) != FSLED_MAGIC)
		return -ENOTTY;

	switch (cmd)
	{
	case FSLED_ON:
		writel(readl(fsled->dat) | (0x1 << fsled->pin), fsled->dat);
		break;
	case FSLED_OFF:
		writel(readl(fsled->dat) & ~(0x1 << fsled->pin), fsled->dat);
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}

static struct file_operations fsled_ops = {
	.owner = THIS_MODULE,
	.open = fsled_open,
	.release = fsled_release,
	.unlocked_ioctl = fsled_ioctl,
};

static int fsled_probe(struct platform_device *pdev)
{
	int ret;
	dev_t dev;
	struct fsled_dev *fsled;
	struct resource *res;
	unsigned int pin = *(unsigned int *)pdev->dev.platform_data;

	dev = MKDEV(FSLED_MAJOR, pdev->id);
	ret = register_chrdev_region(dev, 1, FSLED_DEV_NAME);
	if (ret)
		goto reg_err;

	fsled = kzalloc(sizeof(struct fsled_dev), GFP_KERNEL);
	if (!fsled)
	{
		ret = -ENOMEM;
		goto mem_err;
	}

	cdev_init(&fsled->cdev, &fsled_ops);
	fsled->cdev.owner = THIS_MODULE;
	ret = cdev_add(&fsled->cdev, dev, 1);
	if (ret)
		goto add_err;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
	{
		ret = -ENOENT;
		goto res_err;
	}

	fsled->con = ioremap(res->start, resource_size(res));
	if (!fsled->con)
	{
		ret = -EBUSY;
		goto map_err;
	}
	fsled->dat = fsled->con + 1;

	fsled->pin = pin;
	atomic_set(&fsled->available, 1);
	writel((readl(fsled->con) & ~(0xF << 4 * fsled->pin)) | (0x1 << 4 * fsled->pin), fsled->con);
	writel(readl(fsled->dat) & ~(0x1 << fsled->pin), fsled->dat);
	platform_set_drvdata(pdev, fsled);

	/* 在fsled类下面创建设备led%d，在创建过程中，内核会发送热插拔事件给udev，udev利用这些信息可以创建设备节点 */
	fsled->dev = device_create(fsled_cls, NULL, dev, NULL, "led%d", pdev->id);
	if (IS_ERR(fsled->dev))
	{
		ret = PTR_ERR(fsled->dev);
		goto dev_err;
	}

	return 0;

dev_err:
	iounmap(fsled->con);
map_err:
res_err:
	cdev_del(&fsled->cdev);
add_err:
	kfree(fsled);
mem_err:
	unregister_chrdev_region(dev, 1);
reg_err:
	return ret;
}

static int fsled_remove(struct platform_device *pdev)
{
	dev_t dev;
	struct fsled_dev *fsled = platform_get_drvdata(pdev);

	dev = MKDEV(FSLED_MAJOR, pdev->id);

	device_destroy(fsled_cls, dev);
	iounmap(fsled->con);
	cdev_del(&fsled->cdev);
	kfree(fsled);
	unregister_chrdev_region(dev, 1);

	return 0;
}

struct platform_driver fsled_drv = {
	.driver = {
		.name = "fsled",
		.owner = THIS_MODULE,
	},
	.probe = fsled_probe,
	.remove = fsled_remove,
};

static int __init fsled_init(void)
{
	int ret;

	/* 创建一个名叫fsled的类 */
	fsled_cls = class_create(THIS_MODULE, "fsled");
	if (IS_ERR(fsled_cls))
		return PTR_ERR(fsled_cls);

	ret = platform_driver_register(&fsled_drv);
	if (ret)
		class_destroy(fsled_cls);

	return ret;
}

static void __exit fsled_exit(void)
{
	platform_driver_unregister(&fsled_drv);
	class_destroy(fsled_cls);
}

module_init(fsled_init);
module_exit(fsled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("A simple character device driver for LEDs on FS4412 board");
```

