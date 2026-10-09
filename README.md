# linux-nxx

Chạy **Android 4.4 KitKat** trên **Nintendo Switch** bằng QEMU, đóng gói thành app homebrew `qemu-kitkat-arm.nro`.

QEMU 11.1.2 được port sang Horizon OS (libnx/devkitA64) và chạy native AArch64 trên Switch. Android chạy bên trong
như một máy ảo ARM (`vexpress-a15`, RAM 2 GB, màn hình 1024x576), lệnh được dịch bằng TCG JIT.

> **Trạng thái:** app đã build và đóng gói; Android boot tới màn hình chính khi chạy cùng cấu hình trên macOS.
> **Chưa chạy thử trên Switch thật.** Repo không phát hành sẵn file `.nro` hay ROM: bạn tự build và tự tải ROM.

## Hướng dẫn sử dụng

### 1. Tải ROM Android

Repo không chứa ROM. Dùng image chính thức của Google:

| ROM | Tải về |
| --- | --- |
| Android SDK system image, Android 4.4.2 (API 19), `armeabi-v7a`, bản r05 | <https://dl.google.com/android/repository/sys-img/android/armeabi-v7a-19_r05.zip> |

Không cần tải tay: `arm/make-disks.sh` (bước 2) tự tải file zip này về `images/arm/`.
Kiểm tra: SHA-1 `d1a5fd4f2e1c013c3d3d9bfe7e9db908c3ed56fa`. Image thuộc Google, nằm dưới điều khoản giấy phép Android SDK.

### 2. Build app và tạo ổ đĩa

Cần máy macOS (hoặc Linux) đã cài các công cụ ở mục [Build từ mã nguồn](#build-từ-mã-nguồn).

```sh
git clone https://github.com/ToThangGTVT/linux-nxx.git
cd linux-nxx

scripts/build-all.sh      # build QEMU cho Switch, đóng gói out/qemu-kitkat-arm.nro
arm/build-kernel.sh       # build kernel Android (trong Docker)
arm/make-disks.sh         # tải ROM, tạo ổ đĩa vào out/sdcard/switch/qemu-kitkat-arm/
arm/run-mac.sh            # khuyên chạy 1 lần: Android tối ưu ứng dụng (dexopt) ngay trên Mac,
                          # đỡ cho Switch lần boot đầu nặng nhất; đóng cửa sổ khi đã vào màn hình chính

cp out/qemu-kitkat-arm.nro out/sdcard/switch/
```

### 3. Chép vào thẻ SD

Chép thư mục `out/sdcard/switch/` vào `/switch/` trên thẻ SD (cần khoảng **400 MB**):

```
/switch/qemu-kitkat-arm.nro
/switch/qemu-kitkat-arm/
    zImage, vexpress.dtb, ramdisk.img          kernel, device tree, ramdisk
    system.qcow2, cache.qcow2, userdata.qcow2  ổ đĩa Android
```

### 4. Chạy

1. **Giữ R** khi mở một game bất kỳ để vào hbmenu ở chế độ title takeover (app cần nhiều RAM; mở từ Album sẽ báo lỗi).
2. Chọn **QEMU KitKat ARM**.
3. Chờ Android boot. Màn hình đen trong vài phút đầu là bình thường.
4. Thoát: bấm nút **HOME** của Switch rồi đóng app.

### Điều khiển

| Nút | Android |
| --- | --- |
| Màn hình cảm ứng | touchpad: vuốt để di con trỏ, chạm nhẹ để click |
| Cần analog trái | di con trỏ |
| ZR / ZL | click trái / phải |
| A / B / X / Y | Enter / Back / Home / Menu |
| D-pad | phím mũi tên |
| L / R | Page Up / Page Down |
| + | mở bàn phím ảo của Switch, gõ chữ vào Android |
| − | Esc |

### Tuỳ chỉnh tham số QEMU

Tạo `/switch/qemu-kitkat-arm/args.txt` để thay toàn bộ tham số mặc định. Mỗi dòng có thể chứa một hay nhiều tham số;
`"ngoặc kép"` giữ dấu cách; dòng bắt đầu bằng `#` là chú thích. File mẫu chứa đúng tham số mặc định: `examples/args-arm.txt`.

### Xử lý sự cố

| Hiện tượng | Nguyên nhân / cách xử lý |
| --- | --- |
| Báo "Applet mode only has ~400 MB of memory" | Đang mở từ Album. Thoát, **giữ R khi mở một game** để vào hbmenu |
| App thoát ngay, log có "failed to create ... JIT memory" | Loader không cấp JIT CodeMemory. Cập nhật Atmosphère và hbloader |
| Màn hình đen lâu | Lần boot đầu Android dexopt mọi ứng dụng; trên Switch có thể mất nhiều phút. Chạy `arm/run-mac.sh` một lần trước khi chép ổ đĩa |
| Lỗi khác | Xem log tại `/switch/qemu-kitkat-arm/qemu.log` |

Hiện chưa có **mạng** và **âm thanh**.

---

## Build từ mã nguồn

Cần:

- macOS hoặc Linux.
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) với `devkitA64`, `libnx`, `switch-sdl2`, `switch-zlib`, `switch-mesa`.
- `meson`, `ninja`, `pkg-config`, `python3`.
- `qemu` và `e2fsprogs` (Homebrew) để tạo ổ đĩa và chạy thử trên Mac.
- Docker (OrbStack hoặc Docker Desktop) để build kernel.

`scripts/build-all.sh` tải mã nguồn QEMU, glib, pixman, libffi, pcre2, libiconv; áp các bản vá trong `patches/`;
cross-compile thư viện vào `build/sysroot`; rồi build QEMU và đóng gói `.nro`.

## Cấu trúc

| Đường dẫn | Nội dung |
| --- | --- |
| `nxcompat/` | lớp tương thích POSIX cho Horizon: `mmap` ẩn danh, `pipe` + `poll/select/fcntl` (ld `--wrap`), `pread/pwrite`, termios, `sigsetjmp`, JIT CodeMemory, chia luồng ra 3 nhân, frontend Switch (CPU boost, args.txt, log, bàn phím ảo) |
| `patches/qemu-11.1.2-horizon.patch` | host OS `horizon`, coroutine backend AArch64, TCG split-wx qua JIT libnx, `os-horizon.c`, điều khiển Joy-Con và chế độ touchpad (`ui/sdl2-switch.c`) |
| `patches/glib-2.82.5-horizon.patch` | chỉ build glib/gthread/gmodule, vá vài API thiếu trong newlib |
| `arm/` | cấu hình và bản vá kernel 3.18, build kernel trong Docker, tạo ổ đĩa, vá ramdisk, chạy thử trên Mac |
| `scripts/` | build thư viện và QEMU, đóng gói `.nro` |
| `examples/` | file `args.txt` mẫu |

## Ghi chú kỹ thuật

- Kernel Linux 3.18 tự build, bật driver Android trong staging (binder, ashmem, logger, alarm). Kernel đi kèm image SDK dành
  cho board "goldfish" riêng của Google emulator, QEMU gốc không có.
- `androidboot.hardware=ranchu`: Android gắn system/cache/data từ virtio `vda/vdb/vdc`. QEMU gán `virtio-blk-device` từ
  transport cuối, nên trên dòng lệnh ổ được khai báo ngược (data, cache, system).
- Màn hình PL111 CLCD 1024x576 16bpp (`arm/kernel-patches/`): PL111 giới hạn 1024 px/dòng; driver làm tròn bpp xuống nên
  `max-memory-bandwidth` phải dư; gralloc của Android cần 16bpp.
- Cần `CONFIG_LBDAF` để gắn ext4 có `huge_file` (userdata của SDK).
- Heap Dalvik (`dalvik.vm.heapgrowthlimit=128m`, `heapsize=512m`) đặt trong `default.prop` của ramdisk: emulator của Google
  đặt chúng qua `qemu-props`/`qemud`, QEMU gốc không có, nên mặc định chỉ 16 MB và launcher bị giết vì hết bộ nhớ khi mở
  danh sách ứng dụng.
- `vexpress` chỉ có chuột PS/2 (tương đối), nên màn hình cảm ứng được dùng như touchpad.
