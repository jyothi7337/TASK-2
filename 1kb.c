
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

/*available i2c bus number*/
#define I2C_BUS (1)
/*Page SIZE to write to eeprom*/
#define pg_size 16
/*block size*/
#define BLK_SIZE 256
/*eeprom ic address*/
#define BASE_SLAVE_ADDRESS (0x50)
/*our slave device name*/
#define SLAVE_DEVICE_NAME "eeprom_slave"
/*Kernel(EEPROM) Buffer Size*/
#define MEM_SIZE 1024
/*Kernel(EEPROM) Buffer*/
static char *i2c_buffer;

/*device number*/
static dev_t eeprom_dev;
/*cdev structure*/
static struct cdev i2c_cdev;
/*class structure*/
static struct class *i2c_class;
/*I2C adapter structure*/
static struct i2c_adapter *i2c_adapter;
/*I2C client structure*/
static struct i2c_client *i2c_chardev_client;

/*write location*/
static int loc = 0;

static int arr[] = {0x50, 0x51, 0x52, 0x53};

static int i2c_chardev_open(struct inode *inode, struct file *file)
{

  printk("I2C EEPROM Slave Device Opened...\n");

  i2c_buffer = kzalloc(MEM_SIZE, GFP_KERNEL);
  if (i2c_buffer == NULL)
  {
    pr_err("Failed to allocate memory");
    return -1;
  }

  return 0;
}

static int i2c_chardev_release(struct inode *inode, struct file *file)
{
  kfree(i2c_buffer);
  printk("I2C EEPROM Slave Device Closed\n");
  return 0;
}

static ssize_t i2c_chardev_read(struct file *file, char *__user buf, size_t len, loff_t *off)
{
  int ret = 0;
  int pos = 0;
  int read_len = 0;
  char page_buffer[pg_size] = {0};
  int rem_bytes = len;
  int blk_num = 0;
  int blk_pos = 0;

  memset(i2c_buffer, 0x00, MEM_SIZE);
  blk_num = loc / BLK_SIZE;
  printk("Reading from block: %d\n", blk_num);

  i2c_chardev_client->addr = arr[blk_num];

  blk_pos = loc % BLK_SIZE;

  // location write here
  page_buffer[0] = blk_pos;
  ret = i2c_master_send(i2c_chardev_client, page_buffer, 1);
  if (ret < 0)
  {
    printk("Failed to write in Read\n");
    return -1;
  }
  mdelay(6);
  printk("Byte Write from read: %d\n", ret);

  if (blk_pos % pg_size != 0)
  {
    memset(page_buffer, 0x00, pg_size);
    read_len = ((blk_pos / pg_size) + 1) * pg_size - blk_pos;
    ret = i2c_master_recv(i2c_chardev_client, page_buffer, read_len);
    if (ret < 0)
    {
      pr_err("failed to write first chunk\n");
      return -1;
    }
    mdelay(5);
    printk("ret = %d\n", ret);
    memcpy(i2c_buffer + pos, page_buffer, read_len);
    printk("page_buffer: %s\n", page_buffer);
    pos = pos + read_len;
    rem_bytes = rem_bytes - read_len;
    blk_pos = blk_pos + read_len;
    if (blk_pos >= BLK_SIZE)
    {
      i2c_chardev_client->addr = arr[++blk_num];
      blk_pos = 0;
    }
  }
  while (rem_bytes >= pg_size)
  {
    memset(page_buffer, 0x00, pg_size);
    read_len = pg_size;
    ret = i2c_master_recv(i2c_chardev_client, page_buffer, read_len);
    if (ret < 0)
    {
      pr_err("failed to write in while loop\n");
      return -1;
    }
    mdelay(5);
    printk("ret = %d\n", ret);
    memcpy(i2c_buffer + pos, page_buffer, read_len);
    printk("page_buffer: %s\n", page_buffer);
    pos = pos + read_len;
    rem_bytes = rem_bytes - read_len;
    blk_pos = blk_pos + read_len;
    if (blk_pos >= BLK_SIZE)
    {
      i2c_chardev_client->addr = arr[++blk_num];
      blk_pos = 0;
    }
  }

  if (rem_bytes > 0 && rem_bytes < pg_size)
  {
    memset(page_buffer, 0x00, pg_size);
    read_len = rem_bytes;
    ret = i2c_master_recv(i2c_chardev_client, page_buffer, read_len);
    mdelay(5);
    if (ret < 0)
    {
      pr_err("failed to write in while loop\n");
      return -1;
    }
    printk("ret = %d\n", ret);

    memcpy(i2c_buffer + pos, page_buffer, read_len);
    printk("page_buffer: %s\n", page_buffer);
    pos = pos + read_len;
    rem_bytes = rem_bytes - read_len;
    blk_pos = blk_pos + read_len;
  }

  printk("Reading: %s\n", i2c_buffer);
  if (copy_to_user(buf, i2c_buffer, len) > 0)
  {
    pr_err("Couldn't read all the bytes to user\n");
    return -1;
  }

  return len;
}

static ssize_t i2c_chardev_write(struct file *file, const char *__user buf, size_t len, loff_t *off)
{
  int ret = 0;
  int pos = 0;
  int rem_bytes = len;
  int wr_len = 0;
  int blk_num = 0;
  int blk_pos = 0;

  char page_buffer[pg_size + 1] = {0};

  memset(i2c_buffer, 0x00, MEM_SIZE);

  printk("Writing to EEPROM Slave..\n");

  if (copy_from_user(i2c_buffer, buf, len) > 0)
  {
    pr_err("failed to write\n");
    return -1;
  }

  printk("Writing: %s\n", i2c_buffer);

  blk_num = loc / BLK_SIZE;

  printk("Writing to block: %d\n", blk_num);

  i2c_chardev_client->addr = arr[blk_num];
  printk("i2c_addr = 0x%x", i2c_chardev_client->addr);

  blk_pos = loc % BLK_SIZE;

  if (blk_pos % pg_size != 0)
  {
    memset(page_buffer, 0x00, pg_size + 1);
    wr_len = ((blk_pos / pg_size) + 1) * pg_size - blk_pos;
    page_buffer[0] = blk_pos;
    if (wr_len > len)
    {
      wr_len = len;
    }
    memcpy(page_buffer + 1, i2c_buffer + pos, wr_len);
    printk("page_buffer: %s\n", page_buffer);
    ret = i2c_master_send(i2c_chardev_client, page_buffer, wr_len + 1);
    if (ret < 0)
    {
      pr_err("failed to write first chunk\n");
      return -1;
    }
    mdelay(6);
    printk("ret = %d\n", ret);
    pos = pos + wr_len;
    rem_bytes = rem_bytes - wr_len;
    blk_pos = blk_pos + wr_len;
    if (blk_pos >= BLK_SIZE)
    {
      i2c_chardev_client->addr = arr[++blk_num];
      blk_pos = 0;
    }
  }
  while (rem_bytes >= pg_size)
  {
    memset(page_buffer, 0x00, pg_size + 1);
    wr_len = pg_size;
    page_buffer[0] = blk_pos;
    memcpy(page_buffer + 1, i2c_buffer + pos, wr_len);
    printk("page_buffer: %s\n", page_buffer);
    ret = i2c_master_send(i2c_chardev_client, page_buffer, wr_len + 1);
    if (ret < 0)
    {
      pr_err("failed to write in while loop\n");
      return -1;
    }
    mdelay(6);
    printk("ret = %d\n", ret);
    pos = pos + wr_len;
    rem_bytes = rem_bytes - wr_len;
    blk_pos = blk_pos + wr_len;
    if (blk_pos >= BLK_SIZE)
    {
      i2c_chardev_client->addr = arr[++blk_num];
      blk_pos = 0;
    }
  }

  if (rem_bytes > 0 && rem_bytes < pg_size)
  {
    memset(page_buffer, 0x00, pg_size + 1);
    wr_len = rem_bytes;
    page_buffer[0] = blk_pos;
    memcpy(page_buffer + 1, i2c_buffer + pos, wr_len);
    printk("page_buffer: %s\n", page_buffer);
    ret = i2c_master_send(i2c_chardev_client, page_buffer, wr_len + 1);
    if (ret < 0)
    {
      pr_err("failed to write in last chunk\n");
      return -1;
    }
    mdelay(6);
    printk("ret = %d\n", ret);
    pos = pos + wr_len;
    rem_bytes = rem_bytes - wr_len;
    blk_pos = blk_pos + wr_len;
  }
  return len;
}
loff_t i2c_chardev_llseek(struct file *file, loff_t off, int whence)
{
  loc = (int)off;
  printk("Given Starting location = %d\n", loc);
  return off;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = i2c_chardev_open,
    .read = i2c_chardev_read,
    .write = i2c_chardev_write,
    .llseek = i2c_chardev_llseek,
    .release = i2c_chardev_release,
};

static int i2c_chardev_probe(struct i2c_client *i2c_chardev_client, const struct i2c_device_id *eeprom_ids)
{
  int ret = alloc_chrdev_region(&eeprom_dev, 0, 1, SLAVE_DEVICE_NAME);

  if (ret < 0)
  {
    pr_err("Failed to register i2c eeprom slave device\n");
    return -1;
  }

  cdev_init(&i2c_cdev, &fops);

  if (cdev_add(&i2c_cdev, eeprom_dev, 1) < 0)
  {
    pr_err("Failed i2c eeprom slave cdev_add\n");
    unregister_chrdev_region(eeprom_dev, 1);
    return -1;
  }

  i2c_class = class_create(THIS_MODULE, "eeprom_slave_class");
  if (i2c_class == NULL)
  {
    pr_err("Failed to create i2c eeprom slave class\n");
    cdev_del(&i2c_cdev);
    unregister_chrdev_region(eeprom_dev, 1);
    return -1;
  }

  if (device_create(i2c_class, NULL, eeprom_dev, NULL, SLAVE_DEVICE_NAME) == NULL)
  {
    pr_err("Failed to create i2c eeprom slave device file\n");
    class_destroy(i2c_class);
    cdev_del(&i2c_cdev);
    unregister_chrdev_region(eeprom_dev, 1);
    return -1;
  }

  printk("I2C EEPROM Salve Driver Probed\n");
  return 0;
}

static int i2c_chardev_remove(struct i2c_client *i2c_chardev_client)
{
  device_destroy(i2c_class, eeprom_dev);
  class_destroy(i2c_class);
  cdev_del(&i2c_cdev);
  unregister_chrdev_region(eeprom_dev, 1);
  printk("I2C EEPROM Salve Driver Removed\n");
  return 0;
}

/*i2c slave(eeprom) device id structure*/
static const struct i2c_device_id eeprom_ids[] = {
    {SLAVE_DEVICE_NAME, 0},
    {}};

MODULE_DEVICE_TABLE(i2c, eeprom_ids);

/*i2c driver structure*/
static struct i2c_driver eeprom_driver = {
    .driver = {
        .name = SLAVE_DEVICE_NAME,
        .owner = THIS_MODULE,
    },
    .probe = i2c_chardev_probe,
    .remove = i2c_chardev_remove,
    .id_table = eeprom_ids,
};

/*EEPROM IC information structure*/
static const struct i2c_board_info eeprom_ic_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, BASE_SLAVE_ADDRESS)};

static int __init i2c_chardev_init(void)
{
  i2c_adapter = i2c_get_adapter(I2C_BUS);

  if (i2c_adapter == NULL)
  {
    pr_err("I2C adapter failed to allocate\n");
    return -1;
  }

  i2c_chardev_client = i2c_new_client_device(i2c_adapter, &eeprom_ic_info);

  if (i2c_chardev_client == NULL)
  {
    pr_err("Failed to create new client device for i2c eeprom slave\n");
    return -1;
  }

  i2c_add_driver(&eeprom_driver);

  printk("I2C EEPROM Salve Driver loaded successsfully\n");
  return 0;
}

static void __exit i2c_chardev_exit(void)
{
  i2c_unregister_device(i2c_chardev_client);
  i2c_del_driver(&eeprom_driver);
  printk("I2C EEPROM Salve Driver unloaded successfully\n");
}

module_init(i2c_chardev_init);
module_exit(i2c_chardev_exit);

MODULE_AUTHOR("Jyothirmai");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple I2C EEPROM Slave Device Driver");
MODULE_VERSION("2.0");
