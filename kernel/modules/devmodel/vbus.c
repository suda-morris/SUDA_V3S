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
