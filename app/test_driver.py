import os

DEVICE_PATH = "/dev/cipher_dev"

def encrypt_decrypt(text):
    print(f"Bản gốc: {text}")
    
    # 1. Mở thiết bị ở mức thấp (System call) với quyền Đọc/Ghi
    # Thao tác này sẽ gọi hàm dev_open trong Kernel
    fd = os.open(DEVICE_PATH, os.O_RDWR)
    
    try:
        # 2. Gửi dữ liệu xuống Kernel (gọi dev_write)
        # Phải chuyển chuỗi (string) thành dạng byte thô (encode)
        os.write(fd, text.encode('utf-8'))
        
        # 3. Đọc dữ liệu từ Kernel về (gọi dev_read)
        # Đọc tối đa 1024 bytes
        result_bytes = os.read(fd, 1024)
        
        # Chuyển byte thô ngược lại thành chuỗi (decode)
        return result_bytes.decode('utf-8')
    finally:
        # 4. Đóng thiết bị (gọi dev_release)
        os.close(fd)

if __name__ == "__main__":
    secret_message = "12345678"
    
    # Lần 1: Mã hóa
    encrypted = encrypt_decrypt(secret_message)
    print(f"Đã mã hóa: {encrypted}")
    print("-" * 20)
    
    # Lần 2: Giải mã (đưa chuỗi đã mã hóa vào lại driver)
    decrypted = encrypt_decrypt(encrypted)
    print(f"Đã giải mã: {decrypted}")