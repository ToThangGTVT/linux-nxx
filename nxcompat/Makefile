# libnxcompat: POSIX compatibility layer for Horizon (Nintendo Switch / libnx)
#   make && make install     (installs into $(NXX_SYSROOT))
# Consumers must link with: $(shell pkg-config --libs nxcompat)

include $(DEVKITPRO)/libnx/switch_rules

NXX_SYSROOT ?= $(CURDIR)/../build/sysroot
BUILD := $(CURDIR)/../build/nxcompat
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))
CFLAGS := -g -O2 -Wall -Wextra -Wno-unused-parameter -ffunction-sections \
          -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE \
          -D__SWITCH__ -Iinclude -I$(DEVKITPRO)/libnx/include

WRAPS := poll select fcntl pthread_create

all: $(BUILD)/libnxcompat.a

$(BUILD)/%.o: src/%.c $(wildcard include/*.h include/*/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/libnxcompat.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

install: all
	mkdir -p $(NXX_SYSROOT)/lib/pkgconfig $(NXX_SYSROOT)/include/sys
	cp $(BUILD)/libnxcompat.a $(NXX_SYSROOT)/lib/
	cp include/sys/*.h $(NXX_SYSROOT)/include/sys/
	cp include/*.h $(NXX_SYSROOT)/include/
	mkdir -p $(NXX_SYSROOT)/include/nxcompat && cp include/nxcompat/*.h $(NXX_SYSROOT)/include/nxcompat/
	printf 'prefix=%s\nlibdir=$${prefix}/lib\nincludedir=$${prefix}/include\n\nName: nxcompat\nDescription: POSIX compatibility layer for Horizon\nVersion: 1.0\nLibs: -L$${libdir} -lnxcompat -lnx %s\nCflags: -I$${includedir}\n' \
	  "$(NXX_SYSROOT)" "$(foreach w,$(WRAPS),-Wl,--wrap=$(w))" > $(NXX_SYSROOT)/lib/pkgconfig/nxcompat.pc

clean:
	rm -rf $(BUILD)

.PHONY: all install clean
