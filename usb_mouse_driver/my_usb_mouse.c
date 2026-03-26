#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h> // Để dùng kmalloc (cấp phát bộ nhớ trong Kernel)

#define MIN(a,b) (((a) <= (b)) ? (a) : (b))

// --- CẤU TRÚC DỮ LIỆU CỦA DRIVER ---
struct my_mouse_dev {
    struct usb_device *udev;
    struct urb *irq_urb;        // Cấu trúc URB để nhận dữ liệu ngắt (Interrupt)
    unsigned char *int_buffer;  // Vùng nhớ đệm chứa dữ liệu tọa độ gửi về
    dma_addr_t int_dma;         // Địa chỉ DMA
};

// --- BẢNG NHẬN DIỆN THIẾT BỊ ---
// Cho Kernel biết driver này hỗ trợ thiết bị nào. 
// Ở đây ta dùng class: HID (Human Interface Device), Subclass: Boot, Protocol: Mouse
static const struct usb_device_id mouse_id_table[] = {
    { USB_INTERFACE_INFO(USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 1, 2) },
    { USB_INTERFACE_INFO(USB_CLASS_HID, 1, 2) }, // Giao thức chuẩn của chuột
    { } /* Null terminating entry */
};
MODULE_DEVICE_TABLE(usb, mouse_id_table);

// --- HÀM XỬ LÝ SỰ KIỆN KHI CHUỘT CỬ ĐỘNG (INTERRUPT HANDLER) ---
static void mouse_irq_callback(struct urb *urb)
{
    struct my_mouse_dev *dev = urb->context;
    unsigned char *data = dev->int_buffer;
    int status = urb->status;

    if (status == 0) {
        // Gói dữ liệu chuột thường có 4 bytes (Boot Protocol)
        // Byte 0: Trạng thái nút bấm
        // Byte 1: Trục X
        // Byte 2: Trục Y
        // Byte 3: Con lăn (Scroll)
        
        bool left_click = data[0] & 0x01;
        bool right_click = data[0] & 0x02;
        bool middle_click = data[0] & 0x04;
        signed char x = data[1];
        signed char y = data[2];

        printk(KERN_INFO "USB_MOUSE: [X: %d, Y: %d] | Trai: %d, Phai: %d, Giua: %d\n", 
               x, y, left_click, right_click, middle_click);
    }

    // Gửi lại URB để tiếp tục lắng nghe các cử động tiếp theo
    usb_submit_urb(urb, GFP_ATOMIC);
}

// --- HÀM PROBE (Chạy khi vừa cắm chuột vào máy) ---
static int mouse_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct my_mouse_dev *dev;
    struct usb_endpoint_descriptor *endpoint;
    int pipe, maxp;

    printk(KERN_INFO "USB_MOUSE: Phat hien mot con chuot USB duoc cam vao!\n");

    // 1. Cấp phát bộ nhớ cho cấu trúc quản lý thiết bị
    dev = kzalloc(sizeof(struct my_mouse_dev), GFP_KERNEL);
    if (!dev) return -ENOMEM;

    dev->udev = udev;

    // 2. Tìm Endpoint dạng ngắt (Interrupt Endpoint) để nhận dữ liệu
    if (interface->cur_altsetting->desc.bNumEndpoints < 1) {
        kfree(dev);
        return -ENODEV;
    }
    endpoint = &interface->cur_altsetting->endpoint[0].desc;
    
    if (!usb_endpoint_is_int_in(endpoint)) {
        printk(KERN_ALERT "USB_MOUSE: Khong tim thay Interrupt Endpoint!\n");
        kfree(dev);
        return -ENODEV;
    }

    // 3. Khởi tạo đường ống (Pipe) và bộ đệm (Buffer)
    pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);
    maxp = usb_maxpacket(udev, pipe);

    dev->int_buffer = usb_alloc_coherent(udev, maxp, GFP_KERNEL, &dev->int_dma);
    if (!dev->int_buffer) {
        kfree(dev);
        return -ENOMEM;
    }

    // 4. Khởi tạo URB (USB Request Block)
    dev->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!dev->irq_urb) {
        usb_free_coherent(udev, maxp, dev->int_buffer, dev->int_dma);
        kfree(dev);
        return -ENOMEM;
    }

    // 5. Nạp cấu hình cho URB và liên kết với hàm callback "mouse_irq_callback"
    usb_fill_int_urb(dev->irq_urb, udev, pipe, dev->int_buffer, maxp,
                     mouse_irq_callback, dev, endpoint->bInterval);
    dev->irq_urb->transfer_dma = dev->int_dma;
    dev->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    // Lưu con trỏ dev vào interface để dùng lúc disconnect
    usb_set_intfdata(interface, dev);

    // 6. Kích hoạt URB để bắt đầu lắng nghe chuột
    if (usb_submit_urb(dev->irq_urb, GFP_KERNEL)) {
        printk(KERN_ALERT "USB_MOUSE: Khong the submit URB!\n");
        usb_free_urb(dev->irq_urb);
        usb_free_coherent(udev, maxp, dev->int_buffer, dev->int_dma);
        kfree(dev);
        return -EIO;
    }

    printk(KERN_INFO "USB_MOUSE: Khoi tao thanh cong! Bat dau doc du lieu...\n");
    return 0;
}

// --- HÀM DISCONNECT (Chạy khi rút chuột ra khỏi máy) ---
static void mouse_disconnect(struct usb_interface *interface)
{
    struct my_mouse_dev *dev = usb_get_intfdata(interface);

    printk(KERN_INFO "USB_MOUSE: Chuot USB da bi rut ra!\n");

    if (dev) {
        // Hủy URB và giải phóng toàn bộ bộ nhớ
        usb_kill_urb(dev->irq_urb);
        usb_free_urb(dev->irq_urb);
        usb_free_coherent(dev->udev, 8, dev->int_buffer, dev->int_dma);
        kfree(dev);
    }
}

// Đăng ký các hàm với USB Core của Linux
static struct usb_driver my_mouse_driver = {
    .name = "my_usb_mouse_driver",
    .id_table = mouse_id_table,
    .probe = mouse_probe,
    .disconnect = mouse_disconnect,
};

// --- KHỞI TẠO MODULE ---
static int __init my_mouse_init(void)
{
    printk(KERN_INFO "USB_MOUSE: Dang dang ky driver...\n");
    return usb_register(&my_mouse_driver);
}

static void __exit my_mouse_exit(void)
{
    printk(KERN_INFO "USB_MOUSE: Dang huy dang ky driver...\n");
    usb_deregister(&my_mouse_driver);
}

module_init(my_mouse_init);
module_exit(my_mouse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ban (Sinh vien xuat sac)");
MODULE_DESCRIPTION("Driver doc toa do va click chuot USB thong qua URB");