#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define NEWCHARLED_CNT      1
#define NEWCHARLED_NAME     "newCharDevLed"
#define LEDOFF              0
#define LEDON               1

#define PERIPH_BASE             (0X40000000)
#define MPU_AHB4_PERIPH_BASE    (PERIPH_BASE + 0X10000000)
#define RCC_BASE                (MPU_AHB4_PERIPH_BASE + 0X0000)
#define RCC_MP_AHB4ENSETR       (RCC_BASE + 0XA28)
#define GPIOI_BASE              (MPU_AHB4_PERIPH_BASE + 0XA000)
#define GPIOI_MODER             (GPIOI_BASE + 0X0000)
#define GPIOI_OTYPER            (GPIOI_BASE + 0X0004)
#define GPIOI_OSPEEDR           (GPIOI_BASE + 0X0008)
#define GPIOI_PUPDR             (GPIOI_BASE + 0X000C)
#define GRIOI_BSRR              (GPIOI_BASE + 0X0018)


static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

struct newCharLedDev
{
    dev_t deviceId;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
};

struct newCharLedDev newCharLed;


inline static void setRegister(int index,int clearBit,int setBit,void __iomem * registerAddr)
{
    u32 val = 0;
    val = readl(registerAddr);
    val &= ~(clearBit << index);
    val |= (setBit << index);
    writel(val,registerAddr);
}

void ledSwitch(u8 stat)
{
    if(stat == LEDON)
        setRegister(16,0x0,0x1,GPIOI_BSRR_PI);
    else
        setRegister(0,0x0,0x1,GPIOI_BSRR_PI);
}

ssize_t ledRead (struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}
ssize_t ledWrite (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retVal;
    unsigned char dataBuf[1];
    unsigned char ledStat;

    retVal = copy_from_user(dataBuf,buf,cnt);

    if(retVal < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }
    ledStat = dataBuf[0];
    ledSwitch(ledStat);
    return 0;

}
int ledOpen (struct inode *inode, struct file *filp)
{
    return 0;
}
int ledRelease (struct inode *inode, struct file *filp)
{
    return 0;
}
static struct file_operations newCharLedFops = 
{
    .owner = THIS_MODULE,
    .open = ledOpen,
    .read = ledRead,
    .write = ledWrite,
    .release = ledRelease
};
void ledUnmap(void)
{
    iounmap(GPIOI_MODER_PI);
    iounmap(GPIOI_OTYPER_PI);
    iounmap(GPIOI_OSPEEDR_PI);
    iounmap(GPIOI_PUPDR_PI);
    iounmap(GPIOI_BSRR_PI);
}
/*配置newCharLed结构体，主要是申请一些资源*/
int setDevice(struct newCharLedDev *newCharLed)
{
    /*下面这一坨挺恶心，由于goto语句的原因还不好模块化*/
    /*申请设备号*/
    int retVal = 0;
    if(newCharLed->major)
    {
        newCharLed->deviceId = MKDEV(newCharLed->major,0);
        retVal = register_chrdev_region(newCharLed->deviceId,NEWCHARLED_CNT,NEWCHARLED_NAME);
        if(retVal < 0)
        {
            printk("can't register %s char driver [ret=%d]\r\n",NEWCHARLED_NAME,retVal);
            goto failMap;
        }
    }
    else
    {
        retVal = alloc_chrdev_region(&newCharLed->deviceId,0,NEWCHARLED_CNT,NEWCHARLED_NAME);
        if(retVal < 0)
        {
            printk("can't register %s char driver [ret=%d]\r\n",NEWCHARLED_NAME,retVal);
            goto failMap;
        }
        newCharLed->major = MAJOR(newCharLed->deviceId);
        newCharLed->minor = MINOR(newCharLed->deviceId);
    }
    printk("new char led major = %d,minor = %d\r\n",newCharLed->major,newCharLed->minor);
    /*向内核中添加一个字符设备*/
    newCharLed->cdev.owner = THIS_MODULE;
    cdev_init(&newCharLed->cdev,&newCharLedFops);

    retVal = cdev_add(&newCharLed->cdev,newCharLed->deviceId,NEWCHARLED_CNT);
    if(retVal < 0)
        goto deleteUnregister;
    /*创建一个设备*/
    newCharLed->class = class_create(THIS_MODULE,NEWCHARLED_NAME);
    if(IS_ERR(newCharLed->class))
        goto deleteCdev;
    
    newCharLed->device = device_create(newCharLed->class,NULL,newCharLed->deviceId,NULL,NEWCHARLED_NAME);
    if(IS_ERR(newCharLed->device))
        goto destroyClass;
    
    return 0;


destroyClass:
    class_destroy(newCharLed->class);
deleteCdev:
    cdev_del(&newCharLed->cdev);
deleteUnregister:
    unregister_chrdev_region(newCharLed->deviceId,NEWCHARLED_CNT);
failMap:
    ledUnmap();
    return -EIO;
}
static int __init ledInit(void)
{

    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR,4);
    GPIOI_MODER_PI = ioremap(GPIOI_MODER,4);
    GPIOI_OTYPER_PI = ioremap(GPIOI_OTYPER,4);
    GPIOI_OSPEEDR_PI = ioremap(GPIOI_OSPEEDR,4);
    GPIOI_PUPDR_PI = ioremap(GPIOI_PUPDR,4);
    GPIOI_BSRR_PI = ioremap(GRIOI_BSRR,4);
    /* 1、使能PI时钟 */
    setRegister(8,0x1,0x1,MPU_AHB4_PERIPH_RCC_PI);
    /* 、设置PI0通用的输出模式。*/
    setRegister(0,0x3,0x1,GPIOI_MODER_PI);
    /* 3、设置PI0为推挽模式。*/
    setRegister(0,0x1,0x0,GPIOI_OTYPER_PI);
    /* 4、设置PI0为高速。*/
    setRegister(0,0x3,0x2,GPIOI_OSPEEDR_PI);
    /* 5、设置PI0为上拉。*/
    setRegister(0,0x3,0x1,GPIOI_PUPDR_PI);
    /* 6、默认关闭LED */
    setRegister(0,0x0,0x1,GPIOI_BSRR_PI);
    
    return setDevice(&newCharLed);
}



static void __exit ledExit(void)
{
    device_destroy(newCharLed.class,newCharLed.deviceId);
    class_destroy(newCharLed.class);
    cdev_del(&newCharLed.cdev);
    unregister_chrdev_region(newCharLed.deviceId,NEWCHARLED_CNT);
    ledUnmap();
}
module_init(ledInit);
module_exit(ledExit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree,"Y");