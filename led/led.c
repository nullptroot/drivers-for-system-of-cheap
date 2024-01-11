#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define LED_MAJOR               200
#define LED_NAME                "led"
#define LEDOFF                  1
#define LEDON                   0

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
/*代码复用，
@index：        设置bit位时，需要偏移几位
@clearBit:      清除那些位
@setBit         设置那些位
@registerAddr   要设置的寄存器地址
例如 
registerAddr = 10011001
index = 2
clearBit = 11
setBit = 1
那么最后的地址就是10010101*/
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
static struct file_operations ledFops = 
{
    .owner = THIS_MODULE,
    .open = ledOpen,
    .read = ledRead,
    .write = ledWrite,
    .release = ledRelease
};
void led_unmap(void)
{
    iounmap(GPIOI_MODER_PI);
    iounmap(GPIOI_OTYPER_PI);
    iounmap(GPIOI_OSPEEDR_PI);
    iounmap(GPIOI_PUPDR_PI);
    iounmap(GPIOI_BSRR_PI);
}
static int __init ledInit(void)
{
    int retVal = 0;

    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR,4);
    GPIOI_MODER_PI = ioremap(GPIOI_MODER,4);
    GPIOI_OTYPER_PI = ioremap(GPIOI_OTYPER,4);
    GPIOI_OSPEEDR_PI = ioremap(GPIOI_OSPEEDR,4);
    GPIOI_PUPDR_PI = ioremap(GPIOI_PUPDR,4);
    GPIOI_BSRR_PI = ioremap(GRIOI_BSRR,4);
    /* 2、使能PI时钟 */
    setRegister(8,0x1,0x1,MPU_AHB4_PERIPH_RCC_PI);
    /* 3、设置PI0通用的输出模式。*/
    setRegister(0,0x3,0x1,GPIOI_MODER_PI);
    /* 3、设置PI0为推挽模式。*/
    setRegister(0,0x1,0x0,GPIOI_OTYPER_PI);
    /* 4、设置PI0为高速。*/
    setRegister(0,0x3,0x2,GPIOI_OSPEEDR_PI);
    /* 5、设置PI0为上拉。*/
    setRegister(0,0x3,0x1,GPIOI_PUPDR_PI);
    /* 6、默认关闭LED */
    setRegister(0,0x0,0x1,GPIOI_BSRR_PI);

    retVal = register_chrdev(LED_MAJOR,LED_NAME,&ledFops);
    if(retVal < 0)
    {
        printk("register char device failed!\r\n");
        goto fail_map;
    }
    return 0;
fail_map:
    led_unmap();
    return -EIO;
}



static void __exit ledExit(void)
{
    led_unmap();

    unregister_chrdev(LED_MAJOR,LED_NAME);
}
module_init(ledInit);
module_exit(ledExit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree,"Y");