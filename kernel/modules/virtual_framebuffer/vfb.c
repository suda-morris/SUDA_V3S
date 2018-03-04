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