#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

#define CHARDEVBASE_MAJOR       200
#define CHARDEVBASE_NAME        "charDevBase"

static char readBuf[100];
static char writeBuf[100];
static char kernelData[] = {"kernel data!"};

static int charDevBaseOpen(struct inode *inode,struct file *filp)
{
    printk("welcom open %s\r\n",CHARDEVBASE_NAME);
    return 0;
}

ssize_t charDevBaseRead (struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int retValue = 0;
    memcpy(readBuf,kernelData,sizeof(kernelData));

    retValue = copy_to_user(buf,readBuf,cnt);
    if(retValue == 0)
        printk("kernel send data success\r\n");
    else
        printk("kernel send data failed\r\n");
    return 0;
}

ssize_t charDevBaseWrite (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retValue = 0;
    retValue = copy_from_user(writeBuf,buf,cnt);
    if(retValue == 0)
        printk("kernel revc data success: %s\r\n",writeBuf);
    else
        printk("kernel recv data failed\r\n");
    return 0;
}

int charDevBaseRelease (struct inode *inode, struct file *filp)
{
    printk("Bye bye!\r\n");
    return 0;
}
static struct file_operations charDevBaseFops = 
{
    .owner = THIS_MODULE,
    .open = charDevBaseOpen,
    .read = charDevBaseRead,
    .write = charDevBaseWrite,
    .release = charDevBaseRelease
};

static int __init charDevBaseInit(void)
{
    int retValue = 0;
    retValue = register_chrdev(CHARDEVBASE_MAJOR,CHARDEVBASE_NAME,&charDevBaseFops);
    if(retValue < 0)
        printk("char base device driver regist failed\r\n");
    printk("char base device init success\r\n");
    return 0;
}

static void __init charDevBaseExit(void)
{
    unregister_chrdev(CHARDEVBASE_MAJOR,CHARDEVBASE_NAME);
    printk("char base device exit\r\n");
}

module_init(charDevBaseInit);
module_exit(charDevBaseExit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree,"Y");