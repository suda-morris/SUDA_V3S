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
