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
