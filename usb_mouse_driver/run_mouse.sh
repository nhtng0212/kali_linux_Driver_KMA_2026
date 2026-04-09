#!/bin/bash

echo " KHỞI ĐỘNG MOUSE RADAR ULTIMATE"

echo "[1] Đang dọn dẹp môi trường..."
sudo rmmod usbhid 2>/dev/null
sudo rmmod my_usb_mouse 2>/dev/null
make clean

echo "[2] Đang biên dịch Kernel Driver..."
make

echo "[3] Đang nạp Driver vào Kernel..."
sudo insmod my_usb_mouse.ko

echo "[4] Đang cấp quyền truy cập..."
sudo chmod 666 /dev/usb_mouse_dev

echo "[5] Đang khởi động Giao diện Python..."
python3 mouse_gui.py

