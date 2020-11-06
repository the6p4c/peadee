PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy

OPENOCD ?= openocd
LIBOPENCM3_ROOT = libopencm3

FP_FLAGS = -msoft-float
ARCH_FLAGS = -mcpu=cortex-m3 -mthumb $(FP_FLAGS)

LD_SCRIPT = linker.ld

LDFLAGS = -L$(LIBOPENCM3_ROOT)/lib
LDFLAGS += --static -nostartfiles
LDFLAGS += -T$(LD_SCRIPT)

CFLAGS = $(ARCH_FLAGS)
CFLAGS += -std=c11 -g3 -O0
CFLAGS += -I$(LIBOPENCM3_ROOT)/include
CFLAGS += -fno-common -ffunction-sections -fdata-sections
CFLAGS += -DSTM32F0

LDLIBS = -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
LDLIBS += -lopencm3_stm32f0

FIRMWARE_ELF = firmware.elf
FIRMWARE_BIN = firmware.bin

all: $(FIRMWARE_ELF) $(FIRMWARE_BIN)

$(FIRMWARE_ELF): main.c $(LIBOPENCM3_ROOT)/lib/libopencm3_stm32f0.a
	$(CC) $(CFLAGS) $(LDFLAGS) main.c $(LDLIBS) -o firmware.elf

$(LIBOPENCM3_ROOT)/Makefile:
	git submodule update --init

$(LIBOPENCM3_ROOT)/lib/libopencm3_%.a: $(LIBOPENCM3_ROOT)/Makefile
	$(MAKE) -C $(LIBOPENCM3_ROOT) TARGETS=stm32/f0

$(FIRMWARE_BIN): $(FIRMWARE_ELF)
	$(OBJCOPY) -O binary $< $@
	python ../tools/fw_pad.py

clean:
	$(RM) $(FIRMWARE_ELF) $(FIRMWARE_BIN)

distclean: clean
	$(MAKE) -C $(LIBOPENCM3_ROOT) clean

.PHONY: all clean distclean
