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
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define DTSLED_CNT      1
#define DTSLED_NAME     "dtsLed"
#define LEDOFF              0
#define LEDON               1



static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

struct dtsLedDev
{
    dev_t deviceId;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
};

struct dtsLedDev dtsLed;


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
    filp->private_data = &dtsLed;
    return 0;
}
int ledRelease (struct inode *inode, struct file *filp)
{
    return 0;
}
static struct file_operations dtsLedFops = 
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
/*配置dtsLed结构体，主要是申请一些资源*/
int setDevice(struct dtsLedDev *dtsLed)
{
    /*下面这一坨挺恶心，由于goto语句的原因还不好模块化*/
    /*申请设备号*/
    int retVal = 0;
    if(dtsLed->major)
    {
        dtsLed->deviceId = MKDEV(dtsLed->major,0);
        retVal = register_chrdev_region(dtsLed->deviceId,DTSLED_CNT,DTSLED_NAME);
        if(retVal < 0)
        {
            printk("can't register %s char driver [ret=%d]\r\n",DTSLED_NAME,retVal);
            goto failMap;
        }
    }
    else
    {
        retVal = alloc_chrdev_region(&dtsLed->deviceId,0,DTSLED_CNT,DTSLED_NAME);
        if(retVal < 0)
        {
            printk("can't register %s char driver [ret=%d]\r\n",DTSLED_NAME,retVal);
            goto failMap;
        }
        dtsLed->major = MAJOR(dtsLed->deviceId);
        dtsLed->minor = MINOR(dtsLed->deviceId);
    }
    printk("new char led major = %d,minor = %d\r\n",dtsLed->major,dtsLed->minor);
    /*向内核中添加一个字符设备*/
    dtsLed->cdev.owner = THIS_MODULE;
    cdev_init(&dtsLed->cdev,&dtsLedFops);

    retVal = cdev_add(&dtsLed->cdev,dtsLed->deviceId,DTSLED_CNT);
    if(retVal < 0)
        goto deleteUnregister;
    /*创建一个设备*/
    dtsLed->class = class_create(THIS_MODULE,DTSLED_NAME);
    if(IS_ERR(dtsLed->class))
        goto deleteCdev;
    
    dtsLed->device = device_create(dtsLed->class,NULL,dtsLed->deviceId,NULL,DTSLED_NAME);
    if(IS_ERR(dtsLed->device))
        goto destroyClass;
    
    return 0;


destroyClass:
    class_destroy(dtsLed->class);
deleteCdev:
    cdev_del(&dtsLed->cdev);
deleteUnregister:
    unregister_chrdev_region(dtsLed->deviceId,DTSLED_CNT);
failMap:
    ledUnmap();
    return -EIO;
}

void testOfFunc(struct device_node * nd)
{
    int retVal = 0;
    u32 regData[12];
    struct property *proper;

    proper = of_find_property(nd,"compatible",NULL);
    if(proper == NULL)
        printk("compatible property find failed\r\n");
    else
        printk("compatible = %s\r\n",(char *)proper->value);

    retVal = of_property_read_u32_array(nd,"reg",regData,12);
    if(retVal < 0)
        printk("reg property read failed!\r\n");
    else
    {
        u8 i = 0;
        printk("reg data:\r\n");
        for(i = 0;i < 12; ++i)
            printk("%#X ",regData[i]);
        printk("\r\n");
    }
}

static int __init ledInit(void)
{
    dtsLed.nd = of_find_node_by_path("/stm32mp1_led");
    if(dtsLed.nd == NULL)
    {
        printk("stm32mp1 led node not find!\r\n");
        return -EINVAL;
    }
    else
        printk("stm32mp1 led node find!\r\n");

    testOfFunc(dtsLed.nd);

    /*设置寄存器*/

    MPU_AHB4_PERIPH_RCC_PI = of_iomap(dtsLed.nd, 0);
  	GPIOI_MODER_PI = of_iomap(dtsLed.nd, 1);
	GPIOI_OTYPER_PI = of_iomap(dtsLed.nd, 2);
	GPIOI_OSPEEDR_PI = of_iomap(dtsLed.nd, 3);
	GPIOI_PUPDR_PI = of_iomap(dtsLed.nd, 4);
	GPIOI_BSRR_PI = of_iomap(dtsLed.nd, 5);

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
    
    return setDevice(&dtsLed);
}



static void __exit ledExit(void)
{
    device_destroy(dtsLed.class,dtsLed.deviceId);
    class_destroy(dtsLed.class);
    cdev_del(&dtsLed.cdev);
    unregister_chrdev_region(dtsLed.deviceId,DTSLED_CNT);
    ledUnmap();
}
module_init(ledInit);
module_exit(ledExit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree,"Y");