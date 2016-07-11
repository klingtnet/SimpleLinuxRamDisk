/* RamDisk - source
 *
 * Andreas Linz & Tobias Sekan
 * andreas.linz@stud.htwk-leipzig.de
 * Systemprogrammierung -- WS 2012/13
 * HTWK
 */

#include "ramdisk.h"

/*  TODO
 *  change ioctl to unlocked_ioctl
 *  - rethink use of kernel warning levels
 */

/*  NOTES
 *  list all loaded modules -- lsmod = cat /proc/modules in readable format
 *  view module meta-information -- modinfo <module_name>
    modinfo only finds the module if it's contained in "/lib/modules/`uname -r`/"!
 *  insert module -- insmod <module_name>
    insmod (and modprobe) can send parameters to the loaded module when called:
    insmod example_module.ko paramter=value
    DEPRECATED: (inside the driver source) MODULE_PARM(paramter, type);
    module_param(parametername, type, permission-bits);
 *  remove module -- rmmod <module_name>
 *  print kernel ring-buffer (kernellog) -- dmesg
 *  list of registered devices -- cat /proc/devices
 *  print major and minor numbers of registered devices -- "ls -l /dev", Example:
    crw-rw-rw- 1 root    root      1,   9 Jan 29 09:13 urandom
    c = char device, permissions, # of hard-links, owner, group, major#, minor#
 *
 *  Kernel-headers are under "/usr/src/linux-headers-`uname -r`"
 *  modules are stored under "/lib/modules/`uname -r`/kernel"
 *
 *  The "blocksize" is determined by the kernel and the filesystem,
    otherwise the "sectorsize" is determined by the underlying hardware.
    Default values are: sector=512 Bytes, block=4 kB
 */

static int major;
static struct ramdisk_dev* device_arr = NULL;
static struct hd_geometry rd_geo;   // hdreg.h

static int user_disk_size = 64;
module_param(user_disk_size, int, 0); //  linux/moduleparam.h, http://www.tldp.org/LDP/lkmpg/2.6/html/x323.html
MODULE_PARM_DESC (user_disk_size, "Size in MiB the RamDisk should use.\n");

#ifdef MEDIA_CHANGEABLE
static int rd_open(struct block_device* rd, fmode_t mode)
{
    return 0;
}

static int rd_release(struct gendisk* gd, fmode_t mode)
{
    return 0;
}

// See: http://lwn.net/Articles/423619/
static unsigned int rd_check_events(struct gendisk* gd, unsigned int clearing)
{
    return 0;
}

static int rd_revalidate(struct gendisk* gd)
{
    return 0;
}
#endif // MEDIA_CHANGEABLE

static int rd_ioctl(struct block_device* rd, fmode_t mode, unsigned cmd, unsigned long arg)
{
    struct hd_geometry hdgeo;
    if(cmd == HDIO_GETGEO)  // for backward compatibility, see rd_getgeo
    {
        hdgeo.cylinders = rd_geo.cylinders;
        hdgeo.heads     = rd_geo.heads;
        hdgeo.sectors   = rd_geo.sectors;
        hdgeo.start     = rd_geo.start;
        if(copy_to_user((void __user*) arg, &hdgeo, sizeof hdgeo))
            return -EFAULT;
        return 0;
    }
    return -ENOIOCTLCMD;    // unknown command
}

static int rd_getgeo(struct block_device* bd, struct hd_geometry* hdgeo)
{
    hdgeo->cylinders = rd_geo.cylinders;
    hdgeo->heads     = rd_geo.heads;
    hdgeo->sectors   = rd_geo.sectors;
    hdgeo->start     = rd_geo.start;
    return 0;
}

// See: http://blog.superpat.com/2010/05/04/a-simple-block-driver-for-linux-kernel-2-6-31/
static void rd_request(struct request_queue* q)
{
    struct request *req;

    req = blk_fetch_request(q);
    while (req != NULL) {
        struct ramdisk_dev *rd = req->rq_disk->private_data;

        if (req == NULL || (req->cmd_type != REQ_TYPE_FS)) {
            printk(KERN_NOTICE PRINT_PREFIX"Skip non-CMD request\n");
            __blk_end_request_all(req, -EIO);
            break;
        }

        rd_transfer(rd, blk_rq_pos(req), blk_rq_cur_sectors(req), bio_data(req->bio), rq_data_dir(req));
        if (!__blk_end_request_cur(req, 0))
        {
            req = blk_fetch_request(q);
        }
    }
    return;
}

static void rd_transfer(struct ramdisk_dev* rd, unsigned long sector, unsigned long nsectors, char* buffer, int write)
{
    unsigned long offset = sector * SECTOR_SIZE;
    unsigned long nbytes = nsectors * SECTOR_SIZE;

    if((offset + nbytes) > rd->size)
    {
        printk(KERN_INFO PRINT_PREFIX"Beyond end of disk (%ld %ld - actual disk size %ld)!\n", offset, nbytes, rd->size);
        return;
    }
    if(write)
        memcpy(rd->data + offset, buffer, nbytes);
    else
        memcpy(buffer, rd->data + offset, nbytes);
}

// blkdev.h
static struct block_device_operations rd_ops =
{
    .owner           = THIS_MODULE,
    .getgeo          = rd_getgeo,
    .ioctl           = rd_ioctl    // TODO: https://lkml.org/lkml/2008/1/8/213
    #ifdef MEDIA_CHANGEABLE
    ,
    .open            = rd_open,
    .release         = rd_release,
    .check_events    = rd_check_events,
    .revalidate_disk = rd_revalidate,
    #endif // MEDIA_CHANGEABLE
};

static void setup_device(struct ramdisk_dev* rd, int dev_nr)
{
    if(rd == NULL)
    {
        printk(KERN_ALERT PRINT_PREFIX"No device with number %d\n", dev_nr);
    }
    memset(rd, 0, sizeof (struct ramdisk_dev)); // reset device structure

    if(user_disk_size <= 0)
    {
        printk(KERN_INFO PRINT_PREFIX"%d bad size!\nUsing default -> 64MiB", user_disk_size);
        user_disk_size = 64;
    }
    rd->sectors = (user_disk_size * 1024) * NR_OF_SECTR_PER_KB;
    rd->size    = rd->sectors * SECTOR_SIZE;
    rd->data    = vmalloc(rd->size);
    if(rd->data == NULL)
    {
        printk(KERN_ALERT PRINT_PREFIX"Could not allocate %d MiB of memory!\n", user_disk_size);
        return;
    }
    printk(KERN_INFO PRINT_PREFIX"Allocated: %ld Bytes / %d MiB\n", rd->size, (int) rd->size / (0x1 << 20));

    // RamDisk is a virtual device, but we have to set plausible disc geometry values
    rd_geo.heads     = 16;  // uchar
    rd_geo.sectors   = 32;  // uchar
    rd_geo.start     = 0L;  // ulong
    rd_geo.cylinders = rd->size / (SECTOR_SIZE * rd_geo.heads * rd_geo.sectors);

    spin_lock_init(&rd->lock);  // setup Mutex

    rd->queue = blk_init_queue(rd_request, &rd->lock);  // set the request queue
    if(rd->queue == NULL)
    {
        printk(KERN_ALERT PRINT_PREFIX"Could not allocate request-queue!\n");
        goto free_and_exit;
    }
    blk_queue_logical_block_size(rd->queue, SECTOR_SIZE);
    rd->queue->queuedata = rd;

    rd->gd = alloc_disk(MINOR_COUNT);   // genhd.h
    if(!rd->gd)
    {
        printk(KERN_ALERT PRINT_PREFIX"Disc allocation failure!\n");
        goto free_and_exit;
    }

    rd->gd->major = major;
    rd->gd->first_minor = dev_nr * MINOR_COUNT;
    rd->gd->fops = &rd_ops;
    rd->gd->queue = rd->queue;
    rd->gd->private_data = rd;

    snprintf(rd->gd->disk_name, 32, DEVICENAME"_%c", 'a'+dev_nr);   // set device name
    printk(KERN_INFO PRINT_PREFIX"Diskname: %s\n", rd->gd->disk_name);
    set_capacity(rd->gd, rd->sectors);

    add_disk(rd->gd);

    return;

    free_and_exit:
        if(rd->data)
            vfree(rd->data);
}

static int __init ramdisk_init(void)    // constructor
{
    /*  If register_blkdev is called with 0 as first argument, the kernel
        automatically allocates a major number. After sucessfully registering,
        the DEVICENAME is shown in /proc/devices */
    major = register_blkdev(0, DEVICENAME);  // fs.h
    if(major <= 0)
    {
        printk(KERN_ALERT PRINT_PREFIX"Could not get a major number!\n");
        return -EBUSY;
    }
    else
    {
        printk(KERN_INFO PRINT_PREFIX"Major number %d\n", major);
        // slab.h, GFP_KERNEL - Allocate normal kernel ram.  May sleep.
        device_arr = kmalloc(DEVICE_COUNT * sizeof(struct ramdisk_dev), GFP_KERNEL);
        if(!device_arr)
        {
            unregister_blkdev(major, DEVICENAME);
            return -ENOMEM;
        }
        else
        {
            for(int i = 0; i < DEVICE_COUNT; i++)
            {
                struct ramdisk_dev* rd;
                rd = device_arr + i;
                printk(KERN_INFO PRINT_PREFIX"#%d registered\n", i);
                setup_device(rd, i);
            }
        }
    }
    return 0;
}

static void __exit ramdisk_exit(void)   // destructor
{
    for(int i=0; i < DEVICE_COUNT; i++)
    {
        struct ramdisk_dev* rd = device_arr + i;

        if(rd->gd)
        {
            del_gendisk(rd->gd);
            // put_disk(rd->gd); TODO: Why?
        }
        if(rd->queue)
        {
            blk_cleanup_queue(rd->queue);
        }
        if(rd->data)  // if allocated, deallocate diskspace
        {
            vfree(rd->data);
        }
    }

    unregister_blkdev(major, DEVICENAME);  // fs.h
    kfree(device_arr);
    printk(KERN_INFO PRINT_PREFIX"unregistered\n");

    return;
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);
