#include <linux/module.h>  
#include <linux/init.h>  
#include <linux/fs.h>  
#include <linux/cdev.h>  
#include <linux/types.h>  
#include <mach/regs-gpio.h>  
#include <asm/io.h>  
#include <linux/io.h>  
#include <linux/wait.h>  
#include <linux/sched.h>  
#include <linux/platform_device.h>  
#include <linux/uaccess.h>  
#include <mach/map.h>  
  
#define SECOND_MAJOR 252 /*预设的second的主设备号*/  
static int second_major = SECOND_MAJOR ;  
  
/*second 设备结构体*/  
struct second_dev  
{  
    struct cdev cdev;//cdev结构体  
    atomic_t counter; //一共经历了多少秒  
    struct timer_list s_timer;//设备要使用的定时器  
};  
  
struct second_dev *second_devp;//设备结构体指针  
  
/*定时器处理函数*/  
static void second_timer_handle(unsigned long arg)  
{  
    mod_timer(&second_devp->s_timer, jiffies+HZ);  
    atomic_inc(&second_devp->counter);  
    printk(KERN_NOTICE"current jiffies is %ld\n",jiffies);  
}  
  
/*file open*/  
int second_open(struct inode*inode,struct file *filp)  
{  
    /*init timer*/  
    init_timer(&second_devp->s_timer);  
    second_devp->s_timer.function = &second_timer_handle;  
    second_devp->s_timer.expires = jiffies + HZ;  
  
    add_timer(&second_devp->s_timer); //add timer  
    atomic_set(&second_devp->counter, 0);  
    return 0;  
}  
  
/*file release*/  
int second_release(struct inode*inode,struct file *filp)  
{  
    del_timer(&second_devp->s_timer);  
  
    return 0;  
}  
  
/*global fifo read func*/  
static ssize_t second_read(struct file *filp,char __user *buf,size_t count,loff_t *ppos)  
{  
    int counter;  
  
    counter = atomic_read(&second_devp->counter);  
    if(put_user(counter,(int *)buf))  
        return -EFAULT;  
    else  
        return sizeof(unsigned int);  
}  
  
/*file op struct*/  
static const struct file_operations second_fops =  
{  
    .owner = THIS_MODULE,  
    .open  = second_open,  
    .release = second_release,  
    .read  = second_read,  
};  
  
/*init and register cdev*/  
static void second_setup_cdev(struct second_dev *dev,int index)  
{  
    int err,devno = MKDEV(second_major,index);  
  
    cdev_init(&dev->cdev,&second_fops);  
    dev->cdev.owner = THIS_MODULE;  
    dev->cdev.ops = &second_fops;  
    err = cdev_add(&dev->cdev,devno,1);  
    if(err)  
        printk(KERN_NOTICE"Error %d adding LED %D",err,index);  
}  
  
/*设备驱动模块加载函数*/  
int second_init(void)  
{  
    int ret;  
    dev_t devno = MKDEV(second_major,0);  
  
    /*申请设备号*/  
    if(second_major)  
        ret = register_chrdev_region(devno,1,"second");  
    else  /*动态申请设备号*/  
    {  
        ret = alloc_chrdev_region(devno,0,1,"second");  
        second_major = MAJOR(devno);  
    }  
  
    if(ret <0)  
        return ret;  
  
    /*动态申请设备结构体的内存*/  
    second_devp = kmalloc(sizeof(struct second_dev),GFP_KERNEL);  
    if(!second_devp)  //kmalloc  fail  
    {  
        ret = -ENOMEM;  
        goto fail_malloc;  
    }  
  
    memset(second_devp,0 ,sizeof(struct second_dev));  
    second_setup_cdev( second_devp , 0);  
    return 0;  
      
fail_malloc:unregister_chrdev_region(devno,1);  
      
}  
  
/*module exit*/  
void second_exit(void)  
{  
    cdev_del(&second_devp->cdev);//注销cdev  
    kfree(second_devp);        //free 设备结构体内存  
    unregister_chrdev_region(MKDEV(second_major,0),1);//free device number  
      
}  
  
MODULE_AUTHOR("Song 's book");  
MODULE_LICENSE("GPL");  
module_param(second_major, int,S_IRUGO);//???  
module_init(second_init);  
module_exit(second_exit);  