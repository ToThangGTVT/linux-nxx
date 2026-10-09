# linux-nxx

Chạy **Android 4.4 KitKat** trên **Nintendo Switch** bằng QEMU, đóng gói thành app homebrew (`.nro`).

QEMU 11.1.2 được port sang Horizon OS (libnx/devkitA64) và chạy native AArch64 trên Switch. Android chạy
bên trong như một máy ảo, lệnh của nó được dịch sang ARM bằng TCG JIT.

> **Trạng thái:** cả hai bản `.nro` đã build và đóng gói; Android đã boot tới màn hình chính khi chạy
> cùng cấu hình trên macOS. **Chưa chạy thử trên Switch thật.**
> Repo không phát hành sẵn file `.nro` hay ROM: bạn tự build và tự tải ROM theo hướng dẫn dưới đây.

## Hai bản

| | **ARM** (khuyên dùng) | x86 |
| --- | --- | --- |
| App | `qemu-kitkat-arm.nro` | `qemu-kitkat.nro` |
| Android | 4.4.2, image chính thức của Android SDK (armeabi-v7a, API 19) | Android-x86 4.4-r5 |
| Máy ảo | `vexpress-a15`, RAM 2 GB | PC (`pc`), RAM 1 GB |
| Màn hình | 1024x576 | 800x600 |
| Cảm ứng | touchpad (vuốt để di con trỏ, chạm để click) | chạm thẳng vào vị trí |
| Boot (trên Mac) | ~20 giây | ~35 giây (lần đầu ~3,5 phút) |

Trên Switch sẽ chậm hơn nhiều so với Mac: Cortex-A57 yếu hơn chip Apple M-series khoảng 5–10 lần.

---

## Hướng dẫn sử dụng

### 1. Chuẩn bị Switch

- Switch **đã hack được** (máy đời đầu chưa vá lỗi, hoặc đã gắn modchip), chạy CFW **Atmosphère** với hbmenu.
- Thẻ SD còn trống khoảng **400 MB** cho bản ARM (khoảng 500 MB cho bản x86).
- Phải mở hbmenu ở chế độ **title takeover**: **giữ R khi mở một game bất kỳ**. Nếu mở từ Album (chế độ
  applet), app chỉ có ~400 MB RAM, không đủ cho Android, và sẽ hiện thông báo lỗi.

### 2. Tải ROM Android

Repo không chứa ROM. Tải ở nguồn gốc:

| Bản | ROM | Tải về |
| --- | --- | --- |
| ARM | Android SDK system image, Android 4.4.2 (API 19), `armeabi-v7a`, bản r05 | <https://dl.google.com/android/repository/sys-img/android/armeabi-v7a-19_r05.zip> |
| x86 | Android-x86 4.4-r5 | <https://sourceforge.net/projects/android-x86/files/Release%204.4/android-x86-4.4-r5.iso/download> |

- **Bản ARM:** không cần tải tay. `arm/make-disks.sh` tự tải file zip trên về `images/arm/`.
  Kiểm tra: SHA-1 `d1a5fd4f2e1c013c3d3d9bfe7e9db908c3ed56fa`. Image này của Google, nằm dưới
  điều khoản giấy phép Android SDK.
- **Bản x86:** tải ISO về `images/android-x86-4.4-r5.iso`.
  Bản mình dùng có SHA-256 `d481150d8de1161e1a1d6f270e535b55fc2ecd94c43d9459c9cfe913e000f7c1`.

### 3. Build app và tạo ổ đĩa

Cần máy macOS (hoặc Linux) đã cài các công cụ ở mục [Build từ mã nguồn](#build-từ-mã-nguồn).

```sh
git clone https://github.com/ToThangGTVT/linux-nxx.git
cd linux-nxx

# Build QEMU cho Switch và đóng gói out/qemu-kitkat.nro, out/qemu-kitkat-arm.nro
scripts/build-all.sh

# Bản ARM: build kernel (Docker), tải ROM, tạo ổ đĩa vào out/sdcard/switch/qemu-kitkat-arm/
arm/build-kernel.sh
arm/make-disks.sh
arm/run-mac.sh            # nên chạy 1 lần trên Mac để Android tối ưu ứng dụng trước (dexopt),
                          # đỡ cho Switch phải làm lần boot đầu nặng nhất; tắt cửa sổ khi đã vào màn hình chính

# Bản x86 (tuỳ chọn): tạo ổ đĩa vào out/sdcard/switch/qemu-kitkat/
scripts/make-disk.sh images/android-x86-4.4-r5.iso 4G

cp out/qemu-kitkat*.nro out/sdcard/switch/
```

### 4. Chép vào thẻ SD

Chép **toàn bộ** thư mục `out/sdcard/switch/` vào `/switch/` trên thẻ SD:

```
/switch/qemu-kitkat-arm.nro
/switch/qemu-kitkat-arm/
    zImage, vexpress.dtb, ramdisk.img          kernel, device tree, ramdisk
    system.qcow2, cache.qcow2, userdata.qcow2  ổ đĩa Android
/switch/qemu-kitkat.nro                        (bản x86, tuỳ chọn)
/switch/qemu-kitkat/
    kernel, initrd.img, android.qcow2
```

### 5. Chạy

1. Giữ **R** và mở một game để vào hbmenu (title takeover).
2. Chọn **QEMU KitKat ARM** (hoặc **QEMU KitKat** cho bản x86).
3. Chờ Android boot. Màn hình đen trong vài phút đầu là bình thường.
4. Thoát: bấm nút **HOME** của Switch rồi đóng app.

### Điều khiển

| Nút | Android |
| --- | --- |
| Màn hình cảm ứng | Bản ARM: touchpad (vuốt để di con trỏ, chạm nhẹ để click). Bản x86: chạm thẳng vào vị trí |
| Cần analog trái | di con trỏ |
| ZR / ZL | click trái / phải |
| A / B / X / Y | Enter / Back / Home / Menu |
| D-pad | phím mũi tên |
| L / R | Page Up / Page Down |
| + | mở bàn phím ảo của Switch, gõ chữ vào Android |
| − | Esc |

### Tuỳ chỉnh tham số QEMU

Tạo file `args.txt` trong thư mục của app (ví dụ `/switch/qemu-kitkat-arm/args.txt`) để thay toàn bộ tham số
mặc định. Mỗi dòng có thể chứa một hay nhiều tham số; `"ngoặc kép"` giữ dấu cách; dòng bắt đầu bằng `#` là chú thích.
File mẫu chứa đúng tham số mặc định: `examples/args-arm.txt`, `examples/args-x86.txt`.

### Xử lý sự cố

| Hiện tượng | Nguyên nhân / cách xử lý |
| --- | --- |
| Báo "Applet mode only has ~400 MB of memory" | Đang mở từ Album. Thoát, **giữ R khi mở một game** để vào hbmenu |
| App thoát ngay, log có "failed to create ... JIT memory" | Loader không cấp JIT CodeMemory. Cập nhật Atmosphère và hbloader |
| Màn hình đen lâu | Lần boot đầu Android dexopt mọi ứng dụng; trên Switch có thể mất nhiều phút. Chạy `arm/run-mac.sh` một lần trước khi chép ổ đĩa |
| Lỗi khác | Xem log tại `/switch/qemu-kitkat-arm/qemu.log` (bản x86: `/switch/qemu-kitkat/qemu.log`) |

Hiện chưa có **mạng** và **âm thanh**.

---

## Xem trước trên Mac

Chạy đúng cấu hình của bản Switch bằng QEMU trên macOS (TCG, cửa sổ cocoa):

```sh
brew install qemu e2fsprogs
arm/run-mac.sh        # bản ARM
scripts/run-mac.sh    # bản x86
```

## Build từ mã nguồn

Cần:

- macOS hoặc Linux.
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) với `devkitA64`, `libnx`, `switch-sdl2`, `switch-zlib`, `switch-mesa`.
- `meson`, `ninja`, `pkg-config`, `python3`.
- Để tạo ổ đĩa: `qemu` và `e2fsprogs` (Homebrew).
- Để build kernel ARM: Docker (OrbStack hoặc Docker Desktop).

`scripts/build-all.sh` tải mã nguồn QEMU, glib, pixman, libffi, pcre2, libiconv; áp các bản vá trong `patches/`;
cross-compile thư viện vào `build/sysroot`; rồi build QEMU và đóng gói `.nro`.

## Cấu trúc

| Đường dẫn | Nội dung |
| --- | --- |
| `nxcompat/` | lớp tương thích POSIX cho Horizon: `mmap` ẩn danh, `pipe` + `poll/select/fcntl` (ld `--wrap`), `pread/pwrite`, termios, `sigsetjmp`, JIT CodeMemory, chia luồng ra 3 nhân, frontend Switch (CPU boost, profile theo target, args.txt, log, bàn phím ảo) |
| `patches/qemu-11.1.2-horizon.patch` | host OS `horizon`, coroutine backend AArch64, TCG split-wx qua JIT libnx, `os-horizon.c`, điều khiển Joy-Con và chế độ touchpad (`ui/sdl2-switch.c`) |
| `patches/glib-2.82.5-horizon.patch` | chỉ build glib/gthread/gmodule, vá vài API thiếu trong newlib |
| `scripts/` | build, đóng gói `.nro`, tạo ổ đĩa x86, chạy thử trên Mac |
| `examples/` | file `args.txt` mẫu cho từng bản |
| `arm/` | cấu hình và bản vá kernel 3.18, build kernel trong Docker, tạo ổ đĩa và vá ramdisk cho bản ARM |

## Ghi chú kỹ thuật

**Bản ARM**

- Kernel Linux 3.18 tự build, bật driver Android trong staging (binder, ashmem, logger, alarm). Kernel đi kèm
  image SDK dành cho board "goldfish" riêng của Google emulator, QEMU gốc không có.
- `androidboot.hardware=ranchu`: Android gắn system/cache/data từ virtio `vda/vdb/vdc`. QEMU gán
  `virtio-blk-device` từ transport cuối, nên trên dòng lệnh ổ được khai báo ngược (data, cache, system).
- Màn hình PL111 CLCD 1024x576 16bpp (`arm/kernel-patches/`): PL111 giới hạn 1024 px/dòng; driver làm
  tròn bpp xuống nên `max-memory-bandwidth` phải dư; gralloc của Android cần 16bpp.
- Cần `CONFIG_LBDAF` để gắn ext4 có `huge_file` (userdata của SDK).
- Heap Dalvik (`dalvik.vm.heapgrowthlimit=128m`, `heapsize=512m`) đặt trong `default.prop` của ramdisk:
  emulator của Google đặt chúng qua `qemu-props`/`qemud`, QEMU gốc không có, nên mặc định chỉ 16 MB và
  launcher bị giết vì hết bộ nhớ khi mở danh sách ứng dụng.

**Bản x86**

- Ảnh đĩa là ext4 trần (không bảng phân vùng, không bootloader); QEMU boot thẳng `-kernel/-initrd`, initrd Android-x86
  tự tìm `SRC=/android-4.4-r5`. Tắt `metadata_csum`, `metadata_csum_seed`, `orphan_file` vì kernel 4.0.9 không mount được.
- Cần `pci=nocrs`: kernel KitKat đọc sai ACPI `_CRS` của QEMU 11, dời BAR của VGA khỏi địa chỉ `vesafb` dùng, gây màn hình đen.
