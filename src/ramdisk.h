/* RamDisk - header
 *
 * Andreas Linz & Tobias Sekan
 * andreas.linz@stud.htwk-leipzig.de
 * Systemprogrammierung -- WS 2012/13
 * HTWK
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/vmalloc.h>

#define DEVICENAME          "RamDisk"
#define PRINT_PREFIX        "RamDisk: "
#define MINOR_COUNT         16
#define DEVICE_COUNT        1
#define SECTOR_SIZE         512
#define NR_OF_SECTR_PER_KB  2

// struct definitions
struct ramdisk_dev
{
    int  sectors;                   // # of sectors
    long size;                      // size in Bytes
    u8* data;                       // disk data, unsigned char = u8
    spinlock_t lock;                // Mutex
    struct request_queue* queue;    // device request queue
    struct gendisk *gd;             // Note: can't be allocated directly, instead use alloc_disk(int minors), see genhd.h
};

// function prototypes
/*  Note: in C foo(void) and foo() are different prototypes, because
    foo() will accept an infinite number of arguments instead of foo(void),
    which has no argument. On the other hand in C++ both declarations would be the same!
    SEE: http://stackoverflow.com/questions/42125/function-declaration-isnt-a-prototype
*/
static void setup_device(struct ramdisk_dev*, int);
static int __init ramdisk_init(void);
static void __exit ramdisk_exit(void);
// block device operations
#ifdef MEDIA_CHANGEABLE
static int rd_open(struct block_device*, fmode_t);
static int rd_release(struct gendisk*, fmode_t);
    //static int rd_media_changed(struct gendisk*);   //Note: media_changed is deprecated, use check_events
static unsigned int rd_check_events(struct gendisk*, unsigned int);
static int rd_revalidate(struct gendisk*);
#endif // MEDIA_CHANGEABLE
    //static int rd_ioctl(struct block_device*, fmode_t, unsigned, unsigned long); //Note: ioctl is deprecated, use unlocked_ioctl() or compat_ioctl()
static int rd_ioctl(struct block_device*, fmode_t, unsigned, unsigned long);
static int rd_getgeo(struct block_device*, struct hd_geometry*);
// request handler
static void rd_request(struct request_queue*);
static void rd_transfer(struct ramdisk_dev*, unsigned long, unsigned long, char*, int);

// meta-information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andreas Linz & Tobias Sekan");
MODULE_DESCRIPTION("A simple Linux Ramdisk");
