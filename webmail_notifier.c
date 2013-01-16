/* Webmail Notifier Linux Driver
   Copyright (C) 2013 Nathan Osman

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

/* Utility macro for logging error messages. */
#define WN_ERR(msg, args...) printk(KERN_DEBUG "[webmail_notifier] " msg "\n", ##args)

/* The vendor and product ID of our device. */
#define WN_VENDOR_ID  0x1d34
#define WN_PRODUCT_ID 0x0004

/* List containing the vendor and product IDs of devices
   supported by this module. */
static struct usb_device_id wn_table[] = {
    {
        USB_DEVICE(WN_VENDOR_ID, WN_PRODUCT_ID)
    },
    {}
};
MODULE_DEVICE_TABLE(usb, wn_table);

/* Forward declare the wn_driver instance so that we can
   access it from wn_open. */
static struct usb_driver wn_driver;

/* Utility method to set the device to the specified color.
   Values for red, green, and blue are in the range 0-255
   although the device range is 0-64. Values are interpolated. */
static int wn_set_color(struct usb_device * udev,
                        u8 red, u8 green, u8 blue)
{
    u8 * buf = NULL;
    int retval = -ENOMEM;
    
    /* Allocate 8 bytes for transmission to the device. */
    buf = kzalloc(8, GFP_KERNEL);
    if(!buf)
    {
        WN_ERR("unable to allocate memory for control message");
        goto error;
    }
    
    /* Prepare the contents of the buffer. */
    buf[0] = red / 4;
    buf[1] = green / 4;
    buf[2] = blue / 4;
    buf[6] = 0x1f;
    buf[7] = 0x05;
    
    /* Send the data to the device and free the memory. */
    retval = usb_control_msg(udev,
                             usb_sndctrlpipe(udev, 0),
                             0x09,
                             0x21,
                             0x0200,
                             0,
                             buf,
                             8,
                             2000);
    kfree(buf);
    
error:
    
    return retval;
}

/* Invoked when the device is opened. Stashes a copy of the
   usb_device for later retrieval by wn_write. */
static int wn_open(struct inode * inode,
                   struct file * file)
{
    int subminor = iminor(inode);
    struct usb_interface * interface = usb_find_interface(&wn_driver, subminor);
    
    /* Ensure that the interface is valid. */
    if(!interface)
    {
        WN_ERR("unable to find interface for subminor");
        return -ENODEV;
    }
    
    /* Store a pointer to the usb_device instance with the file. */
    file->private_data = usb_get_intfdata(interface);
    return 0;
}

/* Invoked when data is written to the device. We interpret
   the data as a hex string prefixed with '#'. */
static ssize_t wn_write(struct file * file,
                        const char __user * user_buf,
                        size_t count,
                        loff_t * ppos)
{
    struct usb_device * udev = file->private_data;
    
    /* The return code of this method is the return
       value of wn_set_color to ensure errors cascade. */
    return wn_set_color(udev, 0, 0, 0xff);
}

/* List containing the file operation functions implemented. */
static struct file_operations wn_fops = {
    .owner = THIS_MODULE,
    .open  = wn_open,
    .write = wn_write
};

/* List containing a pointer to the file_operations instance.
   We also define the name of the device file that will be created. */
static struct usb_class_driver wn_class = {
    .name       = "wn%d",
    .fops       = &wn_fops
};

/* Invoked when one of the supported devices is connected. */
static int wn_probe(struct usb_interface * interface,
                    const struct usb_device_id * id)
{
    struct usb_device * udev = interface_to_usbdev(interface);
    
    /* Ensure that the interface exists. */
    if(!udev)
    {
        WN_ERR("interface_to_usbdev returned NULL");
        return -ENODEV;
    }
    
    /* Associate the usb_device with the interface. */
    usb_set_intfdata(interface, udev);
    
    /* Now attempt to actually create the device. */
    return usb_register_dev(interface, &wn_class);
}

/* Invoked when one of the devices is disconnected. */
static void wn_disconnect(struct usb_interface * interface)
{
    /* Deregister the device we created. */
    usb_deregister_dev(interface, &wn_class);
}

/* Driver information table. */
static struct usb_driver wn_driver = {
    .name =       "webmail_notifier",
    .probe =      wn_probe,
    .disconnect = wn_disconnect,
    .id_table =   wn_table
};

/* Invoked when our module is loaded into the kernel. */
static int __init usb_wn_init(void)
{
    /* Attempt to register the USB driver. */
    int retval = usb_register(&wn_driver);
    
    /* Either way, log the status. */
    if(retval)
        WN_ERR("failed to register the webmail_notifier driver");
    
    return retval;
}

/* Invoked when our device is unloaded from the kernel. */
static void __exit usb_wn_exit(void)
{
    /* Remove the USB driver. */
    usb_deregister(&wn_driver);
}

module_init(usb_wn_init);
module_exit(usb_wn_exit);

/* Define the license and provide a brief description of our module. */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nathan Osman <admin@quickmediasolutions.com>");
MODULE_DESCRIPTION("Dream Cheeky Webmail Notifier driver");
MODULE_VERSION("1.0");
