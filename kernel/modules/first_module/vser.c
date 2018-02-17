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
DEFINE_KFIFO(vsfifo,char,32);

static int vser_open(struct inode* inode,struct file* filp){
    return 0;
}

static int vser_release(struct inode* inode,struct file* filp){
    return 0;
}

static ssize_t vser_read(struct file* filp,char __user* buf,size_t count,loff_t* pos){
    unsigned int copied = 0;
    kfifo_to_user(&vsfifo,buf,count,&copied);
    return copied;
}

static ssize_t vser_write(struct file* filp,const char __user* buf,size_t count,loff_t* pos){
    unsigned int copied = 0;
    kfifo_from_user(&vsfifo,buf,count,&copied);
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
    ret = register_chrdev_region(dev,VSER_DEV_CNT,VSER_DEV_NAME);
    if(ret){
        goto reg_err;
    }
    cdev_init(&vsdev,&vser_ops);
    vsdev.owner = THIS_MODULE;
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