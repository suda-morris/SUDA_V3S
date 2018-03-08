# About Device Tree
### Linux设备树的由来

> 在Linux内核源码的ARM体系结构引入设备树之前，相关的BSP代码中充斥了大量的平台设备代码，而这些代码大多是重复的、杂乱的。但需要说明的是，在Linux中，PowerPC和SPARC体系结构很早就使用了设备树，这并不是一个最近才提出的概念。设备树也称作Open Firmware(OF)或FlattenedDevice Tree(FDT)。
>
> Device Tree是一种描述硬件的数据结构，它并没有什么神奇的地方，也不能把所有硬件配置问题都解决掉，它只是提供了一种语言，将硬件配置从Linux内核源码中提取出来。设备树使得目标板和设备变成数据驱动的，他们必须基于传递给内核的数据进行初始化，而不是像以前一样采用硬编码的方式。通过将dts(device tree source)文件编译成dtb二进制文件，供Linux操作系统内核识别目标板卡上的硬件信息。设备树最初是由开放固件(Open Firmware)使用，用来向一个客户程序(通常是一个操作系统)传递数据的通信方法中的一部分内容。客户程序通过设备树在运行时发现设备的拓扑结构，这样就不需要把硬件信息硬编码到程序中。理论上这可以带来较少的代码重复率，使得单个内核镜像能够支持很多硬件平台。



### Linux设备树的目的

> Linux使用设备树的三个主要原因是：

1. 平台识别

> 内核使用设备树中的数据去识别特定的机器(machine)。在机器识别的过程中，setup_arch会调用setup_machine_fdt，后者通过遍历machine_desc链表，选择最匹配设备树数据的machine_desc结构体。这是通过查找设备树根节点的compatible属性，并把它和machine_desc中dt_compat列表中的各项进行比较来决定哪一个machine_desc结构体是最适合的。

```c
//arch/arm/mach-sunxi/sunxi.c
static const char * const sun8i_board_dt_compat[] = {
	"allwinner,sun8i-a23",
	"allwinner,sun8i-a33",
	"allwinner,sun8i-a83t",
	"allwinner,sun8i-h2-plus",
	"allwinner,sun8i-h3",
	"allwinner,sun8i-v3s",
	NULL,
};
//arch/arm/boot/dts/sun8i-v3s-suda.dts
/ {
	model = "SUDA-V3S";
	compatible = "allwinner,sun8i-v3s";
```

2. 实时配置

> 大多数情况下，设备树是固件与内核之间进行数据通信的唯一方式，所以也用于传递实时的或者配置数据给内核，比如内核参数、initrd镜像的地址等。大多数这种数据被包含在设备树的**chosen**节点中。
>
> 在早期的初始化阶段，页表建立之前，体系结构初始化相关的代码会多次联合使用不同的辅助回调函数去调用of_scan_flat_dt来解析设备树数据。of_scan_flat_dt遍历设备树并利用辅助函数来提取需要的信息。通常，early_init_dt_scan_chosen辅助函数用于解析包括内核参数的chosen节点；early_init_dt_scan_root辅助函数用于初始化设备树的地址空间模型；而early_init_scan_memory辅助函数用于决定可用内存的大小和地址。在ARM平台，setup_machine_fdt函数负责在选取到正确的machine_desc结构体之后进行早期的设备树遍历。

```c
chosen{
  bootargs = "console=ttyS0,115200 loglevel=8";
  initrd-start = <0xc8000000>;
  initrd-end = <0xc8200000>;
};
```

3. 设备植入

> 经过板子识别和早期配置数据解析之后，内核进一步进行初始化。期间unflatten_device_tree函数被调用，将设备树数据转换成一种更有效的实时的形式。同时，机器特殊的启动钩子函数也会被调用，例如machine_desc中的init_early函数，init_irq函数，init_machine函数等。其中，init_machine函数会调用``of_platform_populate``函数，其作用是遍历设备树中的节点，**把匹配的节点转换成平台设备**，然后注册到内核中。



###Linux设备树的使用

> 在Linux中，设备树文件的类型有.dts、.dtsi、.dtb，其中dtsi是被包含的设备树源文件，类似于C语言中的头文件；dts是设备树源文件，可以包含其他dtsi文件；通过dtc工具可以讲dts文件编译为dtb文件。
>
> 在系统起来之后，可以在**/sys/firmware/devicetree**中查看实际使用的设备树。
>
> 属性是简单的键值对，它的值可以为空或包含一个任意字节流。

```c
/* 设备树是一个包含节点和属性的简单树状结构，属性就是键值对，而节点可以同时包含属性和子节点 */
/ {/* 根节点 */
    node1 {/* 子节点node1 */
        a-string-property = "A string";/* 文本字符串属性 */
        a-string-list-property = "first string", "second string";/* 字符串列表 */ 
        a-byte-data-property = [0x01 0x23 0x34 0x56];/* 二进制数据 */
        child-node1 {  
            first-child-property;  
            second-child-property = <1>;/* 元组属性，32位无符号整数 */
            a-string-property = "Hello, world";  
        };  
        child-node2 {  
        };  
    };  
    node2 {/* 子节点node2 */
        an-empty-property;  
        a-cell-property = <1 2 3 4>;
        mixed-property = "a string", [0x01 0x23 0x45 0x67], <0x12345678>;/* 混合属性 */
        child-node1 {  
        };  
    };  
};
```

####设备树实例解析

```c
soc {
		compatible = "simple-bus";
		#address-cells = <1>;
		#size-cells = <1>;
		ranges;/* 子节点使用和父节点一样的地址域 */

		i2c0: i2c@01c2ac00 {/* 节点名称是一个<名称>[@<设备地址>]的形式，设备地址通常是访问该设备的主地址，并且该地址也在节点的reg属性中列出 */
			compatible = "allwinner,sun6i-a31-i2c";/* 系统根据该属性来决定使用哪一个设备驱动来绑定到一个设备上，格式为“<制造商>，<型号>” */
			reg = <0x01c2ac00 0x400>;/* 每个可编址设备都有一个reg属性，它是一个元组表，形如：reg=<地址1 长度1 [地址2 长度2] [地址3 长度3]...由于地址和长度字段都是可变大小的变量，需要由父节点的#address-cells和#size-cells属性来声明各个字段的cell的数量 */
			interrupts = <GIC_SPI 6 IRQ_TYPE_LEVEL_HIGH>;
			clocks = <&ccu CLK_BUS_I2C0>;
			resets = <&ccu RST_BUS_I2C0>;
			pinctrl-names = "default";
			pinctrl-0 = <&i2c0_pins>;
			status = "disabled";
			#address-cells = <1>;
			#size-cells = <0>;
		};
}
```

- 需要注意，正确解释一个reg属性，还需要用到**父节点**的**#address-cells**和**#size-cells**的值
- 按照惯例，如果一个节点有reg属性，那么该节点的名字就必须包含设备地址，这个设备地址就是reg属性里第一个地址值
- 关于设备地址：
  1. 内存映射设备：内存映射的设备应该有地址范围，对于32位的地址可以用1个cell来指定地址值，用1个cell来指定范围。而对于64位的地址就应该用2个cell来指定地址值。还有一种内存映射设备的地址表示方式，就是基地址、偏移和长度，这种方式中，地址也是用2个cell来表示。
  2. 非内存映射设备：有些设备没有被映射到CPU的存储器总线上，虽然这些设备可以有一个地址范围，但他们并不是由CPU直接访问。取而代之的是，父设备的驱动程序会代表CPU执行间接访问。这类设备的典型例子有I2C设备，NAND Flash设备等。
  3. 范围(地址转换)：根节点的地址空间是从CPU的视角进行描述的，根节点的直接子节点使用的也是这个地址域，比如chipid@10000000，但是非根节点的直接子节点就没有使用这个地址域，于是需要把这个地址进行转换。**ranges**属性就用于此目的。

```c
gic: interrupt-controller@01c81000 {
			compatible = "arm,cortex-a7-gic", "arm,cortex-a15-gic";
			reg = <0x01c81000 0x1000>,
			      <0x01c82000 0x1000>,
			      <0x01c84000 0x2000>,
			      <0x01c86000 0x2000>;
			interrupt-controller;/* 表示该设备是一个中断控制器 */
			#interrupt-cells = <3>;/* 声明该中断控制器的中断指示符中cell的个数 */
			interrupts = <GIC_PPI 9 (GIC_CPU_MASK_SIMPLE(4) | IRQ_TYPE_LEVEL_HIGH)>;
};

dma: dma-controller@01c02000 {
			compatible = "allwinner,sun8i-v3s-dma";
			reg = <0x01c02000 0x1000>;
			interrupts = <GIC_SPI 50 IRQ_TYPE_LEVEL_HIGH>;/* GIC_SPI表示共享的外设中断，即这个中断由外设产生，可以连接到一个SoC中的多个ARM核；GIC_PPI表示私有的外设中断，即这个中断由外设产生，但只能连接到一个SoC中的一个特定的ARM核。这里的50表示中断号，IRQ_TYPE_LEVEL_HIGH表示中断的触发类型 */
			clocks = <&ccu CLK_BUS_DMA>;
			resets = <&ccu RST_BUS_DMA>;
			#dma-cells = <1>;
};
```

- 描述中断连接需要4个属性：
  1. interrupt-controller:一个空的属性，用来定义该节点是一个接收中断的设备，即是一个中断控制器
  2. \#interrupt-cells:声明该中断控制器的中断指示符中cell的个数
  3. interrupt-parent:指向设备所连接的中断控制器，如果这个设备节点没有该属性，那么该节点继承父节点的这个属性
  4. interrupts:包含一个中断指示符的列表，对应该设备上的每个中断输出信号

```c
/ {
	model = "SUDA-V3S";
	compatible = "allwinner,sun8i-v3s";

	aliases {/* aliases节点用于指定节点的别名，因为引用一个节点要使用全路径，当子节点离根节点越远时，这样节点名就会显得比较冗长，定义一个别名则比较方便 */
		serial0 = &uart0;
		ethernet0 = &emac;
	};

	chosen {/* chosen节点并不代表一个真正的设备，只是作为一个为固件和操作系统之间传递数据的地方，比如引导参数 */
		stdout-path = "serial0:115200n8";
	};
```



#### 设备树与驱动的配合

* 设备树描述

```c
fsled2@11000C40{
  compatible = "fs4412,fsled";
  reg = <0x11000C40 0x8>;
  id = <2>;
  pin = <7>;
};
fsled3@11000C20{
  compatible = "fs4412,fsled";
  reg = <0x11000C20 0x8>;
  id = <3>;
  pin = <0>;
};
fsled4@11401E0{
  compatible = "fs4412,fsled";
  reg = <0x11401E0 0x8>;
  id = <4>;
  pin = <4>;
};
fsled5@11401E0{
  compatible = "fs4412,fsled";
  reg = <0x11401E0 0x8>;
  id = <5>;
  pin = <5>;
};
```

* 驱动代码

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
#include <linux/platform_device.h> //平台设备

#include <linux/of.h> //解析设备树资源

#include "fsled.h"

#define FSLED_MAJOR 256
#define FSLED_DEV_NAME "fsled"

/* 自定义字符设备 */
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

/* 文件操作集合 */
static struct file_operations fsled_ops = {
	.owner = THIS_MODULE,
	.open = fsled_open,
	.release = fsled_release,
	.unlocked_ioctl = fsled_ioctl,
};

/* 匹配平台驱动与设备 */
static int fsled_probe(struct platform_device *pdev)
{
	int ret;
	dev_t dev;
	struct fsled_dev *fsled;
	struct resource *res;

	/* 第一个参数是设备节点对象的地址，第二个参数是属性的名字，第三个参数是回传的属性值 */
	ret = of_property_read_u32(pdev->dev.of_node, "id", &pdev->id);
	if (ret)
		goto id_err;

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

	ret = of_property_read_u32(pdev->dev.of_node, "pin", &fsled->pin);
	if (ret)
		goto pin_err;

	atomic_set(&fsled->available, 1);
	writel((readl(fsled->con) & ~(0xF << 4 * fsled->pin)) | (0x1 << 4 * fsled->pin), fsled->con);
	writel(readl(fsled->dat) & ~(0x1 << fsled->pin), fsled->dat);
	platform_set_drvdata(pdev, fsled);

	return 0;

pin_err:
	iounmap(fsled->con);
map_err:
res_err:
	cdev_del(&fsled->cdev);
add_err:
	kfree(fsled);
mem_err:
	unregister_chrdev_region(dev, 1);
reg_err:
id_err:
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

/* 声明本驱动支持的设备id集合 */
static const struct of_device_id fsled_of_matches[] = {
	{
		.compatible = "fs4412,fsled", //用于和设备树的节点匹配
	},
	{/* sentinel */}};
MODULE_DEVICE_TABLE(of, fsled_of_matches);

/* 平台设备驱动 */
struct platform_driver pdrv = {
	.driver = {
		.name = "fsled",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fsled_of_matches),
	},
	.probe = fsled_probe,
	.remove = fsled_remove,
};

module_platform_driver(pdrv); //展开后包含模块初始化函数和模块清除函数

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Jiang <jiangxg@farsight.com.cn>");
MODULE_DESCRIPTION("A simple character device driver for LEDs on FS4412 board");
```

