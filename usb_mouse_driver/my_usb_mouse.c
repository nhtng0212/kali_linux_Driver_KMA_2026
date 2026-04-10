#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "usb_mouse_dev"
#define CLASS_NAME  "mouse_class"

static int major_num;
static struct class* mouse_class = NULL;
static struct device* mouse_device = NULL;

static unsigned char current_data[5] = {0, 0, 0, 0, 0};

struct my_mouse_dev {
    struct usb_device *udev;
    struct urb *irq_urb;
    unsigned char *int_buffer;
    dma_addr_t int_dma;
};

static const struct usb_device_id mouse_id_table[] = {
    { USB_INTERFACE_INFO(USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 1, 2) },
    { USB_INTERFACE_INFO(USB_CLASS_HID, 1, 2) },
    { } 
};
MODULE_DEVICE_TABLE(usb, mouse_id_table);

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    if (*offset > 0) return 0; 
    if (len < 5) return -EINVAL; 
    
    if (copy_to_user(buffer, current_data, 5)) return -EFAULT;
    
    // Reset tọa độ để chống trôi Radar
    current_data[1] = 0; 
    current_data[2] = 0; 
    current_data[3] = 0; 
    
    *offset += 5;
    return 5;
}

static struct file_operations fops = { .read = dev_read };

// Ép giá trị 16-bit về 8-bit an toàn cho Python
static inline int clamp_to_8bit(int val) {
    if (val > 127) return 127;
    if (val < -128) return -128;
    return val;
}

static void mouse_irq_callback(struct urb *urb) {
    struct my_mouse_dev *dev = urb->context;
    unsigned char *data = dev->int_buffer;
    int len = urb->actual_length;
    int16_t raw_x, raw_y;

    if (urb->status == 0) {
        current_data[0] = data[0]; // Byte 0: Buttons

        // Giả mã chuột gaming/đời mới
        if (len >= 7) {
            // Lấy X từ Byte 1 và Byte 2
            raw_x = (int16_t)((data[2] << 8) | data[1]);
            // Lấy Y từ Byte 3 và Byte 4
            raw_y = (int16_t)((data[4] << 8) | data[3]);
            
            current_data[1] = (unsigned char)clamp_to_8bit(raw_x);
            current_data[2] = (unsigned char)clamp_to_8bit(raw_y);
            
            // Lấy Con lăn (Wheel) từ Byte 5
            current_data[3] = data[5]; 
        } else {
            // Dự phòng cho chuột tiêu chuẩn
            current_data[1] = data[1];
            current_data[2] = data[2];
            if (len >= 4) current_data[3] = data[3];
            else current_data[3] = 0;
        }
    }
    usb_submit_urb(urb, GFP_ATOMIC);
}

static int mouse_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_device *udev = interface_to_usbdev(interface);
    struct my_mouse_dev *dev;
    struct usb_endpoint_descriptor *endpoint;
    int pipe, maxp;

    dev = kzalloc(sizeof(struct my_mouse_dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;
    dev->udev = udev;

    endpoint = &interface->cur_altsetting->endpoint[0].desc;
    pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);
    maxp = usb_maxpacket(udev, pipe);
    
    dev->int_buffer = usb_alloc_coherent(udev, maxp, GFP_KERNEL, &dev->int_dma);
    dev->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
    
    usb_fill_int_urb(dev->irq_urb, udev, pipe, dev->int_buffer, maxp,
                     mouse_irq_callback, dev, endpoint->bInterval);
    dev->irq_urb->transfer_dma = dev->int_dma;
    dev->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    usb_set_intfdata(interface, dev);
    usb_submit_urb(dev->irq_urb, GFP_KERNEL);

    current_data[4] = 1; 
    printk(KERN_INFO "USB_MOUSE: Beken Gaming Mouse Decoder Loaded!\n");
    return 0;
}

static void mouse_disconnect(struct usb_interface *interface) {
    struct my_mouse_dev *dev = usb_get_intfdata(interface);

    current_data[4] = 0; 
    current_data[0] = 0; current_data[1] = 0; 
    current_data[2] = 0; current_data[3] = 0;

    if (dev) {
        usb_kill_urb(dev->irq_urb);
        usb_free_urb(dev->irq_urb);
        usb_free_coherent(dev->udev, 8, dev->int_buffer, dev->int_dma);
        kfree(dev);
    }
    printk(KERN_INFO "USB_MOUSE: Disconnected!\n");
}

static struct usb_driver my_mouse_driver = {
    .name = "my_usb_mouse_driver",
    .id_table = mouse_id_table,
    .probe = mouse_probe,
    .disconnect = mouse_disconnect,
};

static int __init my_mouse_init(void) {
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    mouse_class = class_create(CLASS_NAME);
    mouse_device = device_create(mouse_class, NULL, MKDEV(major_num, 0), NULL, DEVICE_NAME);
    return usb_register(&my_mouse_driver);
}

static void __exit my_mouse_exit(void) {
    usb_deregister(&my_mouse_driver);
    device_destroy(mouse_class, MKDEV(major_num, 0));
    class_destroy(mouse_class);
    unregister_chrdev(major_num, DEVICE_NAME);
}

module_init(my_mouse_init);
module_exit(my_mouse_exit);
MODULE_LICENSE("GPL");