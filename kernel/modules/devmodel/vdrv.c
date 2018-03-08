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
