#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>       // Thao tác với file (cấp phát Major number)
#include <linux/cdev.h>     // Character device
#include <linux/device.h>   // Tạo device node tự động trong /dev
#include <linux/uaccess.h>  // Hàm copy_to_user và copy_from_user
#include <linux/mutex.h>    // Thư viện xử lý đồng thời (Concurrency)

#define DEVICE_NAME "cipher_dev"
#define CLASS_NAME  "cipher_class"
#define BUFFER_SIZE 4096    // Giới hạn 4KB mỗi lần đọc/ghi

// Các biến toàn cục quản lý device
static int major_num;
static struct class* cipher_class  = NULL;
static struct device* cipher_device = NULL;
static struct cdev cipher_cdev;

// Bộ đệm (buffer) trong Kernel để chứa dữ liệu
static char kernel_buffer[BUFFER_SIZE];
static short buffer_size = 0;

// Khai báo Mutex để bảo vệ kernel_buffer khỏi Race Condition
static DEFINE_MUTEX(cipher_mutex);

// --- THUẬT TOÁN CHUYỂN VỊ (Even-Odd Transposition) ---
// Đảo vị trí từng cặp ký tự liền kề. Ví dụ: "1234" -> "2143"
static void transpose_data(char *data, size_t len) {
    size_t i;
    char temp;
    for (i = 0; i < len - 1; i += 2) {
        temp = data[i];
        data[i] = data[i+1];
        data[i+1] = temp;
    }
}

// --- CÁC HÀM GIAO TIẾP USER-KERNEL ---

static int dev_open(struct inode *inodep, struct file *filep){
    // Khóa Mutex: Nếu có ai đang dùng driver, hàm này sẽ block
    if (!mutex_trylock(&cipher_mutex)) {
        printk(KERN_ALERT "CipherDriver: Device dang duoc su dung boi tien trinh khac!\n");
        return -EBUSY;
    }
    printk(KERN_INFO "CipherDriver: Device da mo, Mutex da khoa.\n");
    return 0;
}

// Hàm Write: Nhận dữ liệu gốc từ App -> Mã hóa -> Lưu vào kernel_buffer
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    size_t bytes_to_copy = (len > BUFFER_SIZE) ? BUFFER_SIZE : len;
    
    // Copy an toàn từ User Space xuống Kernel Space
    if (copy_from_user(kernel_buffer, buffer, bytes_to_copy)) {
        return -EFAULT;
    }

    buffer_size = bytes_to_copy;
    printk(KERN_INFO "CipherDriver: Nhan %d bytes tu User. Dang ma hoa...\n", buffer_size);
    
    // Gọi thuật toán chuyển vị
    transpose_data(kernel_buffer, buffer_size);
    
    return bytes_to_copy; 
}

// Hàm Read: Trả dữ liệu đã mã hóa từ kernel_buffer -> App
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    int error_count = 0;
    short bytes_read;
    
    if (buffer_size == 0) return 0; // Không có gì để đọc (báo hiệu EOF)
    
    // Copy an toàn từ Kernel Space lên User Space
    error_count = copy_to_user(buffer, kernel_buffer, buffer_size);
    
    if (error_count == 0) {
        bytes_read = buffer_size;
        printk(KERN_INFO "CipherDriver: Da tra %d bytes cho User.\n", bytes_read);
        buffer_size = 0; // Reset buffer sau khi đọc xong để chuẩn bị cho lượt mới
        return bytes_read; // Trả về số byte thực tế đã đọc được
    } else {
        printk(KERN_ALERT "CipherDriver: Loi! Khong the gui %d bytes.\n", error_count);
        return -EFAULT;
    }
}

static int dev_release(struct inode *inodep, struct file *filep){
    // Mở khóa Mutex cho tiến trình khác dùng
    mutex_unlock(&cipher_mutex);
    printk(KERN_INFO "CipherDriver: Device da dong, Mutex da mo.\n");
    return 0;
}

// Cấu trúc map các hành động với các hàm ở trên
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// --- HÀM KHỞI TẠO VÀ DỌN DẸP MODULE ---

static int __init cipher_init(void){
    printk(KERN_INFO "CipherDriver: Dang khoi tao module...\n");

    // 1. Cấp phát linh động Major Number
    if (alloc_chrdev_region(&major_num, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ALERT "CipherDriver: Khong the cap phat major number\n");
        return -1;
    }

    // 2. Tạo Class
    cipher_class = class_create(CLASS_NAME);
    if (IS_ERR(cipher_class)) {
        unregister_chrdev_region(major_num, 1);
        return PTR_ERR(cipher_class);
    }

    // 3. Tạo Device node (/dev/cipher_dev)
    cipher_device = device_create(cipher_class, NULL, major_num, NULL, DEVICE_NAME);
    if (IS_ERR(cipher_device)) {
        class_destroy(cipher_class);
        unregister_chrdev_region(major_num, 1);
        return PTR_ERR(cipher_device);
    }

    // 4. Khởi tạo và đăng ký Character Device
    cdev_init(&cipher_cdev, &fops);
    if (cdev_add(&cipher_cdev, major_num, 1) < 0) {
        device_destroy(cipher_class, major_num);
        class_destroy(cipher_class);
        unregister_chrdev_region(major_num, 1);
        return -1;
    }

    // 5. Khởi tạo Mutex
    mutex_init(&cipher_mutex);

    printk(KERN_INFO "CipherDriver: Khoi tao thanh cong! San sang ma hoa!\n");
    return 0;
}

static void __exit cipher_exit(void){
    mutex_destroy(&cipher_mutex); // Hủy Mutex
    cdev_del(&cipher_cdev);
    device_destroy(cipher_class, major_num);
    class_destroy(cipher_class);
    unregister_chrdev_region(major_num, 1);
    printk(KERN_INFO "CipherDriver: Da go bo module!\n");
}

module_init(cipher_init);
module_exit(cipher_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tung Nguyen");
MODULE_DESCRIPTION("Driver ma hoa chuyen vi kem Mutex bao ve");
MODULE_VERSION("1.1");