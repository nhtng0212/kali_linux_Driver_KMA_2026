#!/bin/bash

echo " KHỞI ĐỘNG CIPHERSHIFT KERNEL"

echo "[1] Đang dọn dẹp Kernel..."
sudo rmmod cipher_driver 2>/dev/null
make clean

echo "[2] Đang biên dịch Driver Mã hóa..."
make

echo "[3] Đang nạp Driver vào Kernel..."
sudo insmod cipher_driver.ko

echo "[4] Đang cấp quyền truy cập..."
sudo chmod 666 /dev/cipher_dev

echo "[5] Đang kích hoạt môi trường ảo và khởi động App..."
# Chạy App Python trong môi trường ảo
bash -c "source ../ciphershift_env/bin/activate && python3 ../app/file_manager.py"

