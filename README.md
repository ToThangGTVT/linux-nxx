# linux-nxx

QEMU chạy trên **Nintendo Switch** dưới dạng app homebrew (`.nro`) để giả lập **Android 4.4 KitKat**.

- Host: QEMU 11.1.2 được port sang Horizon OS (libnx/devkitA64), chạy native AArch64 trên Switch.
- Guest: Android-x86 4.4-r5 (32-bit x86) trên máy `pc` của QEMU, dịch lệnh bằng TCG JIT.

> Trạng thái: build và đóng gói `.nro` thành công; cấu hình guest đã kiểm chứng trên macOS
> (boot tới màn hình Welcome). **Chưa chạy thử trên Switch thật.**

## Yêu cầu khi chạy

- Switch đã hack được, chạy CFW **Atmosphère** (loader phải cấp JIT CodeMemory).
- Mở hbmenu ở chế độ **title takeover** (giữ **R** khi mở một game). Chế độ applet (Album) chỉ có ~400 MB RAM, không đủ.
- Chép `out/sdcard/switch/` vào `/switch/` trên thẻ SD:

```
/switch/qemu-kitkat.nro
/switch/qemu-kitkat/android.qcow2   ổ đĩa ext4 chứa system.sfs, ramdisk.img, data/
/switch/qemu-kitkat/kernel
/switch/qemu-kitkat/initrd.img
/switch/qemu-kitkat/args.txt        (tuỳ chọn) ghi đè tham số QEMU, xem args.txt.example
/switch/qemu-kitkat/qemu.log        log khi chạy
```

## Điều khiển

| Nút | Android |
| --- | --- |
| Màn hình cảm ứng | chuột tuyệt đối (usb-tablet) |
| Cần analog trái | di con trỏ |
| ZR / ZL | click trái / phải |
| A / B / X / Y | Enter / Back / Home / Menu |
| D-pad | phím mũi tên |
| L / R | Page Up / Page Down |
| + | bàn phím ảo của Switch, gõ chữ vào Android |
| − | Esc |

## Build

Cần: macOS hoặc Linux, [devkitPro](https://devkitpro.org/wiki/Getting_Started) (`devkitA64`, `libnx`,
`switch-sdl2`, `switch-zlib`, `switch-mesa`), `meson`, `ninja`, `pkg-config`, `python3`;
để tạo ảnh đĩa thêm `qemu` và `e2fsprogs` (Homebrew).

```sh
scripts/build-all.sh                                   # tải nguồn, vá, build deps + QEMU i386/arm, đóng gói 2 bản .nro
scripts/make-disk.sh images/android-x86-4.4-r5.iso 4G  # tạo out/sdcard/switch/qemu-kitkat/
arm/make-disks.sh                                      # (bản ARM) tạo out/sdcard/switch/qemu-kitkat-arm/
cp out/qemu-kitkat*.nro out/sdcard/switch/
```

ISO: <https://sourceforge.net/projects/android-x86/files/Release%204.4/android-x86-4.4-r5.iso/download>

Xem trước cùng cấu hình trên macOS (TCG, cửa sổ cocoa): `scripts/run-mac.sh`.

## Cấu trúc

| Đường dẫn | Nội dung |
| --- | --- |
| `nxcompat/` | lớp tương thích POSIX cho Horizon: `mmap` ẩn danh, `pipe` + `poll/select/fcntl` (ld `--wrap`), `pread/pwrite`, termios, `sigsetjmp`, JIT CodeMemory, chia luồng ra 3 nhân, frontend Switch (CPU boost, args.txt, log, bàn phím ảo) |
| `patches/qemu-11.1.2-horizon.patch` | host OS `horizon`, coroutine backend AArch64, TCG split-wx qua JIT libnx, `os-horizon.c`, điều khiển Joy-Con (`ui/sdl2-switch.c`) |
| `patches/glib-2.82.5-horizon.patch` | chỉ build glib/gthread/gmodule, vá vài API thiếu trong newlib |
| `scripts/` | build, đóng gói, tạo ảnh đĩa, chạy thử trên Mac |

## Bản ARM (thử nghiệm)

Guest ARM thay cho x86: image chính thức **Android SDK armeabi-v7a API 19 (4.4.2)** chạy trên máy
`vexpress-a15` của QEMU gốc, với kernel Linux 3.18 tự build (driver Android staging: binder, ashmem,
logger, alarm). Đã boot tới màn hình chính trên macOS (~30 giây từ lần thứ hai). Bản Switch là
`qemu-kitkat-arm.nro` (đã build, **chưa chạy thử trên máy thật**), đọc dữ liệu từ `/switch/qemu-kitkat-arm/`:

```
/switch/qemu-kitkat-arm.nro
/switch/qemu-kitkat-arm/zImage, vexpress.dtb, ramdisk.img
/switch/qemu-kitkat-arm/system.qcow2, cache.qcow2, userdata.qcow2
```

`vexpress` chỉ có chuột PS/2 (tương đối), nên trên Switch màn hình cảm ứng hoạt động như **touchpad**:
vuốt để di con trỏ, chạm nhẹ để click; cần analog trái cũng di con trỏ.

```sh
arm/build-kernel.sh   # build zImage + DTB trong Docker (Debian stretch, gcc 6)
arm/make-disks.sh     # tải image SDK, tạo out/sdcard/switch/qemu-kitkat-arm/
arm/run-mac.sh        # xem trước trên macOS
```

- `androidboot.hardware=ranchu`: Android gắn system/cache/data từ virtio `vda/vdb/vdc`. QEMU gán
  `virtio-blk-device` từ transport cuối, nên trên dòng lệnh ổ được khai báo ngược (data, cache, system).
- Màn hình PL111 CLCD 1024x576 16bpp (`arm/kernel-patches/`): PL111 giới hạn 1024 px/dòng; driver làm
  tròn bpp xuống nên `max-memory-bandwidth` phải dư; gralloc của Android cần 16bpp.
- Cần `CONFIG_LBDAF` để gắn ext4 có `huge_file` (userdata của SDK).

## Ghi chú kỹ thuật

- Ảnh đĩa là ext4 trần (không bảng phân vùng, không bootloader); QEMU boot thẳng `-kernel/-initrd`, initrd Android-x86 tự tìm `SRC=/android-4.4-r5`. Tắt `metadata_csum`, `metadata_csum_seed`, `orphan_file` vì kernel 4.0.9 không mount được.
- Cần `pci=nocrs`: kernel KitKat đọc sai ACPI `_CRS` của QEMU 11, dời BAR của VGA khỏi địa chỉ `vesafb` dùng, gây màn hình đen.
- Lần boot đầu Android dexopt toàn bộ ứng dụng (~3,5 phút trên Mac M-series; trên Switch chậm hơn nhiều).
- Chưa có mạng và âm thanh.
