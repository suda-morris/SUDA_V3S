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
