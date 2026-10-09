# linux-nxx

[![build](https://github.com/ToThangGTVT/linux-nxx/actions/workflows/build.yml/badge.svg)](https://github.com/ToThangGTVT/linux-nxx/actions/workflows/build.yml)

Chạy **Android 4.4 KitKat** trên **Nintendo Switch** bằng QEMU, đóng gói thành app homebrew `qemu-kitkat-arm.nro`.

QEMU 11.1.2 được port sang Horizon OS (libnx/devkitA64) và chạy native AArch64 trên Switch. Android chạy bên trong
như một máy ảo ARM (`vexpress-a15`, RAM 2 GB, màn hình 1024x576), lệnh được dịch bằng TCG JIT.

> **Trạng thái:** Android boot tới màn hình chính và vào được Internet khi chạy cùng cấu hình trên macOS.
> **Chưa chạy thử trên Switch thật.**

## Cách dùng

### 1. Tải app

Tải **`qemu-kitkat-arm.nro`** ở trang [Releases](https://github.com/ToThangGTVT/linux-nxx/releases/latest).
File này do GitHub Actions build tự động từ mã nguồn trong repo.

### 2. Tạo ổ đĩa Android (một lần, trên máy tính)

ROM Android là image chính thức của Google (Android SDK system image 4.4.2, API 19, `armeabi-v7a`). Giấy phép Android SDK
không cho phép phát hành lại, nên repo không kèm ROM: script dưới đây tự tải ROM từ Google rồi tạo ổ đĩa.

| ROM | Nguồn |
| --- | --- |
| `armeabi-v7a-19_r05.zip` (SHA-1 `d1a5fd4f2e1c013c3d3d9bfe7e9db908c3ed56fa`) | <https://dl.google.com/android/repository/sys-img/android/armeabi-v7a-19_r05.zip> |

Cài công cụ:

```sh
# macOS
brew install qemu e2fsprogs
# Ubuntu / Debian
sudo apt install curl unzip cpio qemu-utils e2fsprogs
```

Tạo ổ đĩa:

```sh
git clone https://github.com/ToThangGTVT/linux-nxx.git
cd linux-nxx
arm/make-disks.sh
```

Script tải ROM và kernel (từ Releases), rồi tạo thư mục `out/sdcard/switch/qemu-kitkat-arm/` (khoảng 300 MB).

Tuỳ chọn, chỉ trên macOS: chạy `arm/run-mac.sh` một lần và chờ tới màn hình chính rồi đóng cửa sổ. Android sẽ tối ưu
ứng dụng (dexopt) ngay trên Mac, Switch khỏi phải làm lần boot đầu nặng nhất.

### 3. Chép vào thẻ SD

```
/switch/qemu-kitkat-arm.nro                    file tải ở bước 1
/switch/qemu-kitkat-arm/                       thư mục out/sdcard/switch/qemu-kitkat-arm/ ở bước 2
    zImage, vexpress.dtb, ramdisk.img
    system.qcow2, cache.qcow2, userdata.qcow2
```

### 4. Chạy

1. Kết nối Switch với Wi-Fi nếu muốn Android vào Internet.
2. **Giữ R** khi mở một game bất kỳ để vào hbmenu ở chế độ title takeover (app cần nhiều RAM; mở từ Album sẽ báo lỗi).
3. Chọn **QEMU KitKat ARM** và chờ Android boot. Màn hình đen trong vài phút đầu là bình thường.
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

### Mạng

Android dùng chung kết nối Wi-Fi của Switch (QEMU user-mode networking). Trong Settings của Android **không có mục Wi-Fi**:
image này gốc là của emulator, mạng đi qua cổng Ethernet ảo, nhưng trình duyệt và ứng dụng vẫn vào Internet bình thường.

### Tuỳ chỉnh tham số QEMU

Tạo `/switch/qemu-kitkat-arm/args.txt` để thay toàn bộ tham số mặc định. Mỗi dòng có thể chứa một hay nhiều tham số;
`"ngoặc kép"` giữ dấu cách; dòng bắt đầu bằng `#` là chú thích. File mẫu chứa đúng tham số mặc định: `examples/args-arm.txt`.

### Xử lý sự cố

| Hiện tượng | Nguyên nhân / cách xử lý |
| --- | --- |
| Báo "Applet mode only has ~400 MB of memory" | Đang mở từ Album. Thoát, **giữ R khi mở một game** để vào hbmenu |
| App thoát ngay, log có "failed to create ... JIT memory" | Loader không cấp JIT CodeMemory. Cập nhật Atmosphère và hbloader |
| Màn hình đen lâu | Lần boot đầu Android dexopt mọi ứng dụng; trên Switch có thể mất nhiều phút. Xem bước tuỳ chọn ở mục 2 |
| Không vào được Internet | Kiểm tra Switch đã kết nối Wi-Fi trước khi mở app |
| App bị đóng (lỗi `2168-0002`) | Cuối `qemu.log` có đoạn `*** CRASH`; gửi kèm file mới nhất trong `/atmosphere/crash_reports/` khi báo lỗi |
| Lỗi khác | Xem log tại `/switch/qemu-kitkat-arm/qemu.log` |

Đọc crash dump: địa chỉ `elf+0x...` là offset trong file ELF đi kèm đúng bản `.nro` đó
(`qemu-kitkat-arm.elf.xz` ở Releases, artifact `qemu-kitkat-arm-elf` của lần build CI, hoặc `out/qemu-kitkat-arm.elf`
khi tự build):

```sh
xz -dk qemu-kitkat-arm.elf.xz
aarch64-none-elf-addr2line -fipC -e qemu-kitkat-arm.elf 0x123456 0x234567
```

Hiện chưa có **âm thanh**.

---

## Build từ mã nguồn

Cần:

- macOS hoặc Linux.
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) với `devkitA64`, `libnx`, `switch-sdl2`, `switch-zlib`, `switch-mesa`.
- `meson`, `ninja`, `pkg-config`, `python3`.
- `qemu` và `e2fsprogs` (Homebrew) để tạo ổ đĩa và chạy thử trên Mac.
- Docker (OrbStack hoặc Docker Desktop) để build kernel.

```sh
scripts/build-all.sh      # out/qemu-kitkat-arm.nro
arm/build-kernel.sh       # build/arm/zImage, vexpress-v2p-ca15-tc1.dtb (make-disks.sh sẽ dùng bản này thay vì tải từ Releases)
```

`scripts/build-all.sh` tải mã nguồn QEMU, glib, pixman, libffi, pcre2, libiconv, libslirp; áp các bản vá trong `patches/`;
cross-compile thư viện vào `build/sysroot`; rồi build QEMU và đóng gói `.nro`.

**CI/CD** (`.github/workflows/build.yml`): mỗi lần push hoặc mở PR, GitHub Actions build `.nro` (container
`devkitpro/devkita64`), file ELF tương ứng (để tra địa chỉ crash) và kernel (Docker), kết quả nằm ở mục Artifacts
của lần chạy. Đẩy một tag `v*` (ví dụ `git tag v0.2.0 && git push origin v0.2.0`) sẽ tạo GitHub Release kèm
`qemu-kitkat-arm.nro`, `qemu-kitkat-arm.elf.xz`, `zImage`, `vexpress.dtb`.

## Cấu trúc

| Đường dẫn | Nội dung |
| --- | --- |
| `nxcompat/` | lớp tương thích POSIX cho Horizon: `mmap` ẩn danh, `pipe` + `poll/select/fcntl` (ld `--wrap`), `pread/pwrite`, termios, `sigsetjmp`, JIT CodeMemory, chia luồng ra 3 nhân, frontend Switch (CPU boost, args.txt, log, bộ ghi crash, bàn phím ảo) |
| `patches/qemu-11.1.2-horizon.patch` | host OS `horizon`, coroutine backend AArch64, TCG split-wx qua JIT libnx, `os-horizon.c`, điều khiển Joy-Con và chế độ touchpad (`ui/sdl2-switch.c`) |
| `patches/glib-2.82.5-horizon.patch` | chỉ build glib/gthread/gmodule, vá vài API thiếu trong newlib |
| `patches/libslirp-4.9.1-horizon.patch` | lấy DNS của Switch qua `nifm` (Horizon không có `/etc/resolv.conf`) |
| `.github/workflows/` | CI/CD: build `.nro` và kernel, tạo Release khi có tag |
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
- Mạng: card LAN9118 của `vexpress` + QEMU user-mode networking (libslirp); `init.goldfish.sh` đặt `eth0 = 10.0.2.15`.
  Trên emulator thật, kết nối dữ liệu di động (RIL) báo DNS cho netd; ở đây không có RIL nên ramdisk có thêm dịch vụ
  `ranchu-net` gọi `ndc resolver` để đặt DNS `10.0.2.3` cho `eth0`.
- Tắt `CONFIG_FRAMEBUFFER_CONSOLE`: con trỏ nhấp nháy của console kernel vẽ đè lên màn hình Android.
