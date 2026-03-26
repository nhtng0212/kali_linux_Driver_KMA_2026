import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from tkinterdnd2 import TkinterDnD, DND_FILES
import os
import threading
import shutil

DEVICE_PATH = "/dev/cipher_dev"
CHUNK_SIZE = 4096
PREVIEW_SIZE = 1024

def format_size(size):
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size < 1024.0:
            return f"{size:.2f} {unit}"
        size /= 1024.0

class CipherShiftPro:
    def __init__(self, root):
        self.root = root
        self.root.title("CipherShift - Secure Kernel File Manager")
        self.root.geometry("950x600")
        self.root.configure(bg="#FFFFFF")

        self.state = {
            'encrypt': {'file': None, 'temp': None},
            'decrypt': {'file': None, 'temp': None}
        }
        self.widgets = {'encrypt': {}, 'decrypt': {}}

        self.setup_ui()

    def setup_ui(self):
        # --- HEADER ---
        header = tk.Frame(self.root, bg="#FFFFFF")
        header.pack(fill=tk.X, padx=20, pady=10)

        lbl_title = tk.Label(header, text="HỆ THỐNG BẢO MẬT DỮ LIỆU CẤP KERNEL", 
                             font=("Segoe UI", 18, "bold"), bg="#FFFFFF", fg="#2C3E50")
        lbl_title.pack(side=tk.LEFT)

        lbl_dev = tk.Label(header, text=f"Driver: {DEVICE_PATH}", 
                           font=("Segoe UI", 11, "bold"), bg="#FFFFFF", fg="#27AE60")
        lbl_dev.pack(side=tk.RIGHT)

        # --- TABS STYLE (SỬA LỖI PHÓNG TO TAB) ---
        style = ttk.Style()
        style.theme_use('clam')
        style.configure("TNotebook", background="#FFFFFF")
        
        # Tab bình thường (Không được chọn): Nhỏ hơn một chút, màu xám
        style.configure("TNotebook.Tab", background="#F2F3F4", font=("Segoe UI", 11, "bold"), padding=[15, 8])
        
        # Tab được chọn: Phóng to padding, tràn viền (expand), màu xanh dương nổi bật
        style.map("TNotebook.Tab", 
                  background=[("selected", "#3498DB")], 
                  foreground=[("selected", "#FFFFFF")],
                  padding=[("selected", [25, 12])], # Phóng to không gian bên trong
                  expand=[("selected", [1, 1, 1, 0])] # Ép nở ra ngoài viền
                  )

        notebook = ttk.Notebook(self.root)
        notebook.pack(fill=tk.BOTH, expand=True, padx=15, pady=5)

        tab_enc = tk.Frame(notebook, bg="#FFFFFF")
        tab_dec = tk.Frame(notebook, bg="#FFFFFF")

        notebook.add(tab_enc, text="  🔒 Bảo mật (Mã hóa)  ")
        notebook.add(tab_dec, text="  🔓 Khôi phục (Giải mã)  ")

        self.build_tab(tab_enc, 'encrypt')
        self.build_tab(tab_dec, 'decrypt')

    def build_tab(self, parent, op_type):
        left_frame = tk.Frame(parent, bg="#FFFFFF", width=350)
        left_frame.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)
        left_frame.pack_propagate(False)

        right_frame = tk.Frame(parent, bg="#F9F9F9", bd=1, relief=tk.SOLID)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)

        # --- TIÊU ĐỀ RÕ RÀNG (Đã bỏ chữ "CHẾ ĐỘ:") ---
        mode_text = "🔒 MÃ HÓA BẢO MẬT" if op_type == 'encrypt' else "🔓 GIẢI MÃ KHÔI PHỤC"
        mode_color = "#E74C3C" if op_type == 'encrypt' else "#8E44AD"
        lbl_mode = tk.Label(left_frame, text=mode_text, font=("Segoe UI", 14, "bold"), fg=mode_color, bg="#FFFFFF", anchor="w")
        lbl_mode.pack(fill=tk.X, pady=(0, 10))

        # --- VÙNG CLICK HOẶC KÉO THẢ ---
        btn_select = tk.Button(left_frame, text="📥 KÉO THẢ FILE VÀO ĐÂY\nhoặc Click để chọn", 
                               font=("Segoe UI", 11, "bold"), bg="#FBFCFC", fg="#7F8C8D", 
                               relief=tk.GROOVE, bd=2, height=4, cursor="hand2",
                               command=lambda: self.select_file(op_type))
        btn_select.pack(fill=tk.X, pady=(0, 15))

        btn_select.drop_target_register(DND_FILES)
        btn_select.dnd_bind('<<Drop>>', lambda e, op=op_type: self.handle_drop(e, op))
        left_frame.drop_target_register(DND_FILES)
        left_frame.dnd_bind('<<Drop>>', lambda e, op=op_type: self.handle_drop(e, op))

        lbl_name = tk.Label(left_frame, text="Tên file: Chưa chọn", bg="#FFFFFF", fg="#34495E", font=("Segoe UI", 10), anchor="w")
        lbl_name.pack(fill=tk.X)

        lbl_size = tk.Label(left_frame, text="Dung lượng: N/A", bg="#FFFFFF", fg="#34495E", font=("Segoe UI", 10), anchor="w")
        lbl_size.pack(fill=tk.X)

        tk.Label(left_frame, text="", bg="#FFFFFF").pack(pady=5)

        progress = ttk.Progressbar(left_frame, orient="horizontal", mode="determinate")

        text_act = "MÃ HÓA NGAY" if op_type == 'encrypt' else "GIẢI MÃ NGAY"
        btn_action = tk.Button(left_frame, text=text_act, font=("Segoe UI", 12, "bold"),
                               bg="#2ECC71", fg="white", relief=tk.FLAT, height=2, state=tk.DISABLED, cursor="hand2",
                               command=lambda: self.process_file(op_type))
        btn_action.pack(fill=tk.X, pady=10)

        btn_export = tk.Button(left_frame, text="💾 XUẤT FILE KẾT QUẢ", font=("Segoe UI", 12, "bold"),
                               bg="#3498DB", fg="white", relief=tk.FLAT, height=2, state=tk.DISABLED, cursor="hand2",
                               command=lambda: self.export_file(op_type))
        btn_export.pack(fill=tk.X)

        txt_preview = tk.Text(right_frame, wrap=tk.WORD, font=("Consolas", 10), bg="#F9F9F9", fg="#2C3E50", bd=0, padx=10, pady=10)
        txt_preview.pack(fill=tk.BOTH, expand=True)
        txt_preview.insert(tk.END, "Nội dung file sẽ hiển thị tại đây...\n(Chỉ preview 1KB đầu để tối ưu hiệu suất)")
        txt_preview.config(state=tk.DISABLED)

        txt_preview.drop_target_register(DND_FILES)
        txt_preview.dnd_bind('<<Drop>>', lambda e, op=op_type: self.handle_drop(e, op))

        self.widgets[op_type] = {
            'btn_select': btn_select, 'lbl_name': lbl_name, 'lbl_size': lbl_size,
            'progress': progress, 'btn_action': btn_action, 'btn_export': btn_export,
            'txt_preview': txt_preview
        }

    # --- LOGIC XỬ LÝ FILE ---
    def select_file(self, op_type):
        path = filedialog.askopenfilename(title="Chọn file xử lý")
        if path:
            self.load_selected_file(path, op_type)

    def handle_drop(self, event, op_type):
        files = self.root.tk.splitlist(event.data)
        if files:
            path = files[0]
            self.load_selected_file(path, op_type)

    def load_selected_file(self, path, op_type):
        self.state[op_type]['file'] = path
        self.state[op_type]['temp'] = None
        w = self.widgets[op_type]

        w['btn_select'].config(
            text="🔄 ĐÃ CHỌN FILE\nClick hoặc Kéo thả để đổi file khác", 
            bg="#EBF5FB", fg="#2980B9"
        )

        w['lbl_name'].config(text=f"Tên file: {os.path.basename(path)}")
        size = os.path.getsize(path)
        w['lbl_size'].config(text=f"Dung lượng: {format_size(size)}")

        w['btn_action'].config(state=tk.NORMAL)
        w['btn_export'].config(state=tk.DISABLED)
        w['progress'].pack_forget() 

        self.load_preview(path, w['txt_preview'], "GỐC")

    def load_preview(self, path, widget, prefix_type):
        widget.config(state=tk.NORMAL)
        widget.delete(1.0, tk.END)
        try:
            with open(path, 'rb') as f:
                data = f.read(PREVIEW_SIZE)

            header = f"--- [NỘI DUNG {prefix_type} (PREVIEW 1KB)] ---\n\n"
            try:
                content = data.decode('utf-8')
                widget.insert(tk.END, header + content)
            except UnicodeDecodeError:
                hex_str = data.hex(' ', 2)
                widget.insert(tk.END, header + "[File Binary / Dữ liệu đã mã hóa]\nHEX VIEW:\n" + hex_str)
        except Exception as e:
            widget.insert(tk.END, f"Không thể đọc file: {e}")
        widget.config(state=tk.DISABLED)

    def process_file(self, op_type):
        path = self.state[op_type]['file']
        w = self.widgets[op_type]

        w['btn_action'].config(state=tk.DISABLED)
        w['btn_select'].config(state=tk.DISABLED)
        w['progress'].pack(fill=tk.X, before=w['btn_action'], pady=(0, 10))
        w['progress']['value'] = 0

        threading.Thread(target=self.kernel_worker, args=(path, op_type), daemon=True).start()

    def kernel_worker(self, path, op_type):
        temp_out = path + ".tmp_kernel"
        try:
            file_size = os.path.getsize(path)
            processed = 0

            with open(path, 'rb') as f_in, open(temp_out, 'wb') as f_out:
                fd_driver = os.open(DEVICE_PATH, os.O_RDWR)

                while True:
                    chunk = f_in.read(CHUNK_SIZE)
                    if not chunk: break

                    read_len = len(chunk)
                    is_padded = False
                    
                    if read_len % 2 != 0:
                        chunk += b'\x00'
                        is_padded = True

                    os.write(fd_driver, chunk)
                    processed_chunk = os.read(fd_driver, len(chunk))

                    if is_padded:
                        processed_chunk = processed_chunk[:-1]

                    f_out.write(processed_chunk)
                    processed += read_len
                    percent = int((processed / file_size) * 100) if file_size > 0 else 100

                    self.root.after(0, self.update_progress, op_type, percent)

                os.close(fd_driver)

            self.root.after(0, self.process_success, op_type, temp_out)

        except Exception as e:
            self.root.after(0, self.process_error, op_type, str(e))

    def update_progress(self, op_type, percent):
        self.widgets[op_type]['progress']['value'] = percent

    def process_success(self, op_type, temp_out):
        self.state[op_type]['temp'] = temp_out
        w = self.widgets[op_type]
        
        w['btn_select'].config(state=tk.NORMAL)
        w['btn_export'].config(state=tk.NORMAL)

        messagebox.showinfo("Hoàn tất", "Xử lý bằng Kernel thành công!\nHãy ấn XUẤT FILE để lưu kết quả.")
        self.load_preview(temp_out, w['txt_preview'], "SAU KHI XỬ LÝ QUA KERNEL")

    def process_error(self, op_type, err):
        w = self.widgets[op_type]
        w['btn_select'].config(state=tk.NORMAL)
        w['btn_action'].config(state=tk.NORMAL)
        messagebox.showerror("Lỗi Kernel", f"Quá trình thất bại:\n{err}\nĐảm bảo đã chạy 'sudo chmod 666 /dev/cipher_dev'")

    def export_file(self, op_type):
        temp_path = self.state[op_type]['temp']
        orig_path = self.state[op_type]['file']

        if op_type == 'encrypt':
            sug_name = os.path.basename(orig_path) + ".cipher"
        else:
            sug_name = os.path.basename(orig_path).replace(".cipher", "")

        save_path = filedialog.asksaveasfilename(initialfile=sug_name, title="Lưu file kết quả")
        if save_path:
            try:
                shutil.move(temp_path, save_path)
                messagebox.showinfo("Thành công", f"Đã xuất file tại:\n{save_path}")
                self.widgets[op_type]['btn_export'].config(state=tk.DISABLED)
                self.state[op_type]['temp'] = None
                
                self.widgets[op_type]['btn_select'].config(
                    text="📥 KÉO THẢ FILE VÀO ĐÂY\nhoặc Click để chọn", 
                    bg="#FBFCFC", fg="#7F8C8D"
                )
                self.widgets[op_type]['lbl_name'].config(text="Tên file: Chưa chọn")
                self.widgets[op_type]['lbl_size'].config(text="Dung lượng: N/A")
                self.widgets[op_type]['txt_preview'].config(state=tk.NORMAL)
                self.widgets[op_type]['txt_preview'].delete(1.0, tk.END)
                self.widgets[op_type]['txt_preview'].insert(tk.END, "Nội dung file sẽ hiển thị tại đây...\n(Chỉ preview 1KB đầu để tối ưu hiệu suất)")
                self.widgets[op_type]['txt_preview'].config(state=tk.DISABLED)
                self.widgets[op_type]['btn_action'].config(state=tk.DISABLED)

            except Exception as e:
                messagebox.showerror("Lỗi Lưu File", str(e))

if __name__ == "__main__":
    if not os.path.exists(DEVICE_PATH):
        print(f"[-] LỖI: Không tìm thấy Driver {DEVICE_PATH}")
        print("[-] Vui lòng nạp driver bằng lệnh: sudo insmod driver/cipher_driver.ko")
        import sys; sys.exit(1)

    root = TkinterDnD.Tk()
    app = CipherShiftPro(root)
    root.mainloop()