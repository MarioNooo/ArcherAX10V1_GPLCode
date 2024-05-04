/********************************************************************
 *This is flash ioctl for software upgrade and user config.
 *******************************************************************/


#include <linux/init.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>

#include <board.h>
#include <bcmTag.h>
#include <bcm_map_part.h>
#include <flash_api.h>
/*
 * IOCTL Command Codes
 */
#define BCM_FLASH_READ				0x01
#define BCM_FLASH_WRITE				0x02
#define BCM_FLASH_ERASE				0x03

#define BCM_IO_MAGIC 				0xB3
#define	BCM_IO_FLASH_READ			_IOR(BCM_IO_MAGIC, BCM_FLASH_READ, char)
#define BCM_IO_FLASH_WRITE			_IOW(BCM_IO_MAGIC, BCM_FLASH_WRITE, char)
#define	BCM_IO_FLASH_ERASE			_IO (BCM_IO_MAGIC, BCM_FLASH_ERASE)

#define	BCM_IOC_MAXNR				14
#define flash_major      				239
#define flash_minor      				0

long bcm_flash_ioctl(struct file *file,  unsigned int cmd, unsigned long arg);
int bcm_flash_open (struct inode *inode, struct file *file);

struct file_operations flash_device_op = {
        .owner = THIS_MODULE,
        .unlocked_ioctl = bcm_flash_ioctl,
        .open = bcm_flash_open,
};

static struct cdev flash_device_cdev = {
		//.kobj   = {.name = "bcm_flash_chrdev", },
        .owner  = THIS_MODULE,
		.ops = &flash_device_op,
};

typedef struct 
{
	u_int32_t addr;		/* flash r/w addr	*/
	u_int32_t len;		/* r/w length		*/
	u_int8_t* buf;		/* user-space buffer*/
	u_int32_t buflen;	/* buffer length	*/
	u_int32_t hasHead;	/* hasHead flag 		*/
}ARG;

#define BCM_FLASH_SECTOR_SIZE	(64 * 1024)

/* protect flash dev ioctl */
static struct semaphore flash_sem;

static int __bcm_flash_read(unsigned int addr, unsigned int len, unsigned char *buf)
{
	
	int bytes = 0;
	while (len > 0)
	{
		bytes = kerSysReadFromFlash(buf, addr, len); 
		if (bytes  < 0)
		{
			printk("bcm_flash_ioctl Read Flash Error\n");
			return -1;
		}
		len -= bytes;
		addr += bytes;
		buf += bytes;
	}
	
	return 0;
	
}

static int __bcm_flash_write(unsigned int addr, unsigned int len, unsigned char *buf)
{
	int ret = 0;
	int remainBytes = 0;
	int bytes = 0;

	while (len > 0)
	{
		if ((remainBytes = kerSysWriteToFlash(addr, (char*)buf, len)) < 0)
		{
			ret = len - remainBytes;
			printk("bcm_flash_ioctl write error\n");
			goto  bad;
		}
		
		bytes = len - remainBytes;			
		len -= bytes;
		addr += bytes;
		buf += bytes;
	}

	printk("#");
	return len;

bad:
	return ret;
}

static int __bcm_flash_erase(unsigned int addr, unsigned int len)
{
	int ret = 0;
	int bytes = 0;

	if ((bytes = kerSysEraseFlash(addr, len)) < 0) {
		ret = bytes;
		goto bad;
	}

	printk("*");
	return bytes;
		
bad:
	return ret;
}


int nvram_flash_write(u_int8_t *tempData, 
    u_int32_t hasHead, u_int32_t offset, u_int8_t *data, u_int32_t len)
{
	u_int32_t address = 0;
	u_int32_t headLen = 0;
	u_int32_t endAddr = 0, startAddr = 0;
	u_int8_t *orignData = NULL;
	u_int32_t headData[2] = {len, 0};
	u_int32_t frontLen = 0, tailLen = 0;
	u_int32_t retLen = 0;

	headData[0] = htonl(len);	

	if (hasHead != FALSE)
	{
		headLen = 2 * sizeof(u_int32_t);
		len += headLen;
	}

	frontLen = offset % BCM_FLASH_SECTOR_SIZE;
	tailLen  = (offset + len) % BCM_FLASH_SECTOR_SIZE;
	/* 第一个block起始地址 */
	address = offset - frontLen;
	/* 最后一个不完整的block起始地址，如果没有，则是再下一个block起始地址 */
	endAddr = offset + len - tailLen;

	orignData = tempData + BCM_FLASH_SECTOR_SIZE;

	if (frontLen > 0 || headLen > 0)/* 第一个block */
	{
		retLen = __bcm_flash_read((uint) address, BCM_FLASH_SECTOR_SIZE, orignData);
		//ath_flash_read(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, orignData);
		memcpy(tempData, orignData, frontLen);/* 前面一部分为原来的的数据 */
		
		if (BCM_FLASH_SECTOR_SIZE < frontLen + headLen) /* 头部被拆分到两个blcok */
		{
			headLen = BCM_FLASH_SECTOR_SIZE - frontLen;
			/* 分区头部，第一部分 */
			memcpy(tempData + frontLen, headData, headLen);

			/***************************************************/
			if (memcmp(orignData, tempData, BCM_FLASH_SECTOR_SIZE)) /*内容变化*/
			{
				__bcm_flash_erase(address, BCM_FLASH_SECTOR_SIZE);
				retLen = __bcm_flash_write((uint)address, BCM_FLASH_SECTOR_SIZE, tempData);
				//ath_flash_sector_erase(address);
				//ath_flash_write(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, tempData);
			}
			address += BCM_FLASH_SECTOR_SIZE;
			/***************************************************/
			retLen = __bcm_flash_read((uint) address, BCM_FLASH_SECTOR_SIZE, orignData);
			//ath_flash_read(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, orignData);
			/* 分区头部，第二部分 */
			memcpy(tempData, (u_int8_t*)(headData) + headLen, 8 - headLen);

			if (len - headLen < BCM_FLASH_SECTOR_SIZE) /*写入数据长度小于一个block*/
			{
				headLen = 8 - headLen;
				copy_from_user(tempData + headLen, data, tailLen - headLen);/* 需要写入的数据 */
				memcpy(tempData + tailLen, orignData + tailLen, BCM_FLASH_SECTOR_SIZE - tailLen);/* 原来的数据 */
				data += tailLen - headLen;
			}
			else
			{
				headLen = 8 - headLen;
				copy_from_user(tempData + headLen, data, BCM_FLASH_SECTOR_SIZE - headLen);/* 需要写入的数据 */
				data += BCM_FLASH_SECTOR_SIZE - headLen;
			}
		}
		else /* 头部未被拆分 */
		{
			memcpy(tempData + frontLen, headData, headLen);/* 分区头部(如果有的话) */
			
			if (len + frontLen < BCM_FLASH_SECTOR_SIZE) /*写入数据长度小于一个block*/
			{
				copy_from_user(tempData + frontLen + headLen, data, len - headLen);/* 后面为需要写入的数据 */
				data += len - headLen;
				/* 再后面是原来的数据 */
				memcpy(tempData + frontLen + len,
						orignData + frontLen + len,
						BCM_FLASH_SECTOR_SIZE - (frontLen + len));
			}
			else
			{
				copy_from_user(tempData + frontLen + headLen, data, BCM_FLASH_SECTOR_SIZE - frontLen - headLen);
				/* 后面为需要写入的数据 */
				data += BCM_FLASH_SECTOR_SIZE - frontLen - headLen;
			}
		}

		/***************************************************/
		if (memcmp(orignData, tempData, BCM_FLASH_SECTOR_SIZE)) /*内容变化*/
		{
			__bcm_flash_erase(address, BCM_FLASH_SECTOR_SIZE);
			retLen = __bcm_flash_write((uint)address, BCM_FLASH_SECTOR_SIZE, tempData);
		    //ath_flash_sector_erase(address);
		    //ath_flash_write(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, tempData);
		}
		address += BCM_FLASH_SECTOR_SIZE;
		/***************************************************/
	}

	if (address < endAddr)/* 中间完整的block，注意:此处用 < 而不是 <=。 */
	{
		startAddr = address;
		while (address < endAddr)
		{
			retLen = __bcm_flash_read((uint) address, BCM_FLASH_SECTOR_SIZE, orignData);
			//ath_flash_read(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, orignData);
			copy_from_user(tempData, data, BCM_FLASH_SECTOR_SIZE);
			/***************************************************/
			if (memcmp(orignData, tempData, BCM_FLASH_SECTOR_SIZE)) /*内容变化*/
			{
				__bcm_flash_erase(address, BCM_FLASH_SECTOR_SIZE);
				retLen = __bcm_flash_write((uint)address, BCM_FLASH_SECTOR_SIZE, tempData);
				//ath_flash_sector_erase(address);
				//ath_flash_write(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, tempData);
			}
			address += BCM_FLASH_SECTOR_SIZE;
			/***************************************************/
			data += BCM_FLASH_SECTOR_SIZE;
		}
	}

	if (address < offset + len) /* 如果还没有写完，则说明最后有一个不完整的block */
	{
		retLen = __bcm_flash_read((uint) address, BCM_FLASH_SECTOR_SIZE, orignData);
		//ath_flash_read(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, orignData);
		copy_from_user(tempData, data, tailLen);/*前面一部分为需要写入的数据*/
		memcpy(tempData + tailLen, orignData + tailLen, BCM_FLASH_SECTOR_SIZE - tailLen);
		/*后面为原来数据*/
		/***************************************************/
		if (memcmp(orignData, tempData, BCM_FLASH_SECTOR_SIZE)) /*内容变化*/
		{
			__bcm_flash_erase(address, BCM_FLASH_SECTOR_SIZE);
			retLen = __bcm_flash_write((uint)address, BCM_FLASH_SECTOR_SIZE, tempData);
		    //ath_flash_sector_erase(address);
		    //ath_flash_write(mtd, address, BCM_FLASH_SECTOR_SIZE, &retLen, tempData);
		}
		address += BCM_FLASH_SECTOR_SIZE;
		/***************************************************/
	}

	return 0;
}


static int __bcm_flash_ioctl(struct file *file,  unsigned int cmd, unsigned long arg)
{
	/* temp buffer for r/w */
	unsigned char *rw_buf = (unsigned char *)kmalloc(2 * BCM_FLASH_SECTOR_SIZE, GFP_KERNEL);
	ARG *parg = (ARG*)arg;
	u_int8_t* usr_buf = parg->buf;
	u_int32_t usr_buflen = parg->buflen;
	u_int32_t addr = parg->addr;
	u_int32_t hasHead = parg->hasHead;
	int ret = 0;
	
	//int nsector = usr_buflen >> 16; 			/* Divide BCM_FLASH_SECTOR_SIZE */	
	//int oddlen = usr_buflen & 0x0000FFFF;	    /* odd length (0 ~ BCM_FLASH_SECTOR_SIZE) */
	
	if (rw_buf == NULL)
	{
		printk("rw_buf error\n");
		goto wrong;
	}
	if (_IOC_TYPE(cmd) != BCM_IO_MAGIC)
	{
		printk("cmd type error!\n");
		goto wrong;
	}
	if (_IOC_NR(cmd) > BCM_IOC_MAXNR)
	{
		printk("cmd NR error!\n");
		goto wrong;
	}

	if (ret < 0)
	{ 
		printk("access no ok!\n");
		goto wrong;
	}
	switch(cmd)
	{
		case BCM_IO_FLASH_READ:
		{
			u_int32_t read_len = usr_buflen;
			u_int32_t bytes = BCM_FLASH_SECTOR_SIZE;

			while (read_len>0)
			{
				if ( bytes >= read_len)
					bytes = read_len;

				if (__bcm_flash_read((uint) addr, bytes, rw_buf) < 0)
				{
					printk("Read Error\n");
					goto wrong;
				}
				if (copy_to_user(usr_buf, rw_buf,bytes)<0)
				{
					printk("copy to user error\n");
					goto wrong;
				}

				read_len -= bytes;
				addr += bytes;
				usr_buf += bytes;
				
			}
			goto good;
			break;
		}
		case BCM_IO_FLASH_WRITE:
			nvram_flash_write(rw_buf, hasHead, addr, usr_buf, usr_buflen);
			break;

		case  BCM_FLASH_ERASE:
		{
			goto good;
			break;
		}
	}

good:
	kfree(rw_buf);
	return 0;
wrong:
	if (rw_buf)
	{
		kfree(rw_buf);
	}

	return -1;
}

long bcm_flash_ioctl(struct file *file,  unsigned int cmd, unsigned long arg)

{
	int retval = 0;

	/* ioctl maybe sleep ! */
	down(&flash_sem);
	retval = __bcm_flash_ioctl(file, cmd, arg);
	up(&flash_sem);

	return retval;
}

int bcm_flash_open (struct inode *inode, struct file *filp)
{
	int minor = iminor(inode);
	//int devnum = minor; //>> 1;
	
	if ((filp->f_mode & 2) && (minor & 1)) {
		printk("You can't open the RO devices RW!\n");
		return -EACCES;
	}
	return 0;	
}

#ifdef CONFIG_TPLINK_MEM_ROOTFS
static struct mtd_info *mtdram_info = NULL;

static int mtdram_read(struct mtd_info *mtd, loff_t from, size_t len,
		size_t *retlen, u_char *buf)
{
	if (from + len > mtd->size)
		return -EINVAL;

	memcpy(buf, mtd->priv + from, len);

	*retlen = len;
	return 0;
}

static void mtdram_cleanup(void)
{
    if (mtdram_info) {
        del_mtd_device(mtdram_info);
        kfree(mtdram_info);
        mtdram_info = NULL;
    }
}

static int mtdram_setup(void)
{
    struct mtd_info *mtd;
    extern unsigned int mtd_ram_addr;
    extern unsigned int mtd_ram_size;

    if (mtd_ram_addr == 0 || mtd_ram_size == 0) {
        return 0;
    }

    printk(KERN_NOTICE "############################################################\n");
    printk(KERN_NOTICE "\n%s: booting with mem rootfs@%x/%x.\n\n",
        __func__, mtd_ram_addr, mtd_ram_size);
    printk(KERN_NOTICE "############################################################\n");

    mtd = get_mtd_device_nm("rootfs");
    if (mtd != NULL && mtd != ERR_PTR(-ENODEV)) {
        put_mtd_device(mtd);
        del_mtd_device(mtd);
    } else {
        return -ENODEV;
    }
    
	mtd = kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
	memset(mtd, 0, sizeof(*mtd));

	mtd->name = "rootfs";
	mtd->type = MTD_ROM;
	mtd->flags = MTD_CAP_ROM;
	mtd->size = mtd_ram_size;
	mtd->writesize = 1;
	mtd->priv = (void*)mtd_ram_addr;

	mtd->owner = THIS_MODULE;
	mtd->read = mtdram_read;

    mtdram_info = mtd;
	if (add_mtd_device(mtd)) {
		return -EIO;
	}

	return 0;
}
#endif

int __init bcm_flash_chrdev_init (void)
{
    dev_t dev;
    int ret = 0;
    int err;
    int bcm_flash_major = flash_major;
    int bcm_flash_minor = flash_minor;

	printk(KERN_WARNING "flash_chrdev :  flash_chrdev_init \n");

    if (bcm_flash_major) {
        dev = MKDEV(bcm_flash_major, bcm_flash_minor);
        ret = register_chrdev_region(dev, 1, "flash_chrdev");
    }
	else {
        ret = alloc_chrdev_region(&dev, bcm_flash_minor, 1, "flash_chrdev");
        bcm_flash_major = MAJOR(dev);
    }

    if (ret < 0) {
        printk(KERN_WARNING "flash_chrdev : can`t get major %d\n", bcm_flash_major);
        goto fail;
    }

    cdev_init (&flash_device_cdev, &flash_device_op);
    err = cdev_add(&flash_device_cdev, dev, 1);
    if (err) 
		printk(KERN_NOTICE "Error %d adding flash_chrdev ", err);

	/* Initialize ioctl semaphore */
	sema_init(&flash_sem, 1);

#ifdef CONFIG_TPLINK_MEM_ROOTFS
	mtdram_setup();
#endif

	return 0;

fail:
	return ret;
}

static void __exit cleanup_bcm_flash_chrdev_exit (void)
{
#ifdef CONFIG_TPLINK_MEM_ROOTFS
	mtdram_cleanup();
#endif
//	unregister_chrdev_region(MKDEV(flash_major, flash_minor), 1);
}

module_init(bcm_flash_chrdev_init);
module_exit(cleanup_bcm_flash_chrdev_exit);
//MODULE_LICENSE("GPL");

