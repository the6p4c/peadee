PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy

OPENOCD ?= openocd

LIBOPENCM3_ROOT = lib/libopencm3
GD32F1X0_FWL_ROOT = lib/GD32F1x0_Firmware_Library_v3.1.0
CMSIS_ROOT = lib/CMSIS

GD32F1X0_SOURCES = $(wildcard $(GD32F1X0_FWL_ROOT)/Firmware/GD32F1x0_standard_peripheral/Source/*.c)
GD32F1X0_OBJECTS = $(GD32F1X0_SOURCES:.c=.o)

FP_FLAGS = -msoft-float
ARCH_FLAGS = -mcpu=cortex-m3 -mthumb $(FP_FLAGS)

LD_SCRIPT = linker.ld

LDFLAGS = -L$(LIBOPENCM3_ROOT)/lib
LDFLAGS += -L$(GD32F1X0_FWL_ROOT)
LDFLAGS += --static -nostartfiles
LDFLAGS += -T$(LD_SCRIPT)

CFLAGS = $(ARCH_FLAGS)
CFLAGS += -std=c11 -g3 -O0
CFLAGS += -I.
CFLAGS += -I$(CMSIS_ROOT)/CMSIS/Core/Include
CFLAGS += -I$(GD32F1X0_FWL_ROOT)/Firmware/CMSIS
CFLAGS += -I$(GD32F1X0_FWL_ROOT)/Firmware/CMSIS/GD/GD32F1x0/Include
CFLAGS += -I$(GD32F1X0_FWL_ROOT)/Firmware/GD32F1x0_standard_peripheral/Include
CFLAGS += -fno-common -ffunction-sections -fdata-sections
CFLAGS += -DGD32F1X0 -DGD32F130_150

LDLIBS = -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
LDLIBS2 = $(LDLIBS)
LDLIBS2 += -lopencm3_gd32f1x0 -lgd32f1x0_fwl

FIRMWARE_ELF = firmware.elf
FIRMWARE_BIN = firmware.bin

all: $(FIRMWARE_ELF) $(FIRMWARE_BIN)

$(FIRMWARE_ELF): main.c $(LIBOPENCM3_ROOT)/lib/libopencm3_gd32f1x0.a $(GD32F1X0_FWL_ROOT)/libgd32f1x0_fwl.a
	$(CC) $(CFLAGS) $(LDFLAGS) main.c $(LDLIBS2) -o firmware.elf

$(LIBOPENCM3_ROOT)/Makefile:
	git submodule update --init

$(LIBOPENCM3_ROOT)/lib/libopencm3_%.a: $(LIBOPENCM3_ROOT)/Makefile
	$(MAKE) -C $(LIBOPENCM3_ROOT) TARGETS=gd32/f1x0

$(GD32F1X0_FWL_ROOT)/libgd32f1x0_fwl.a: $(GD32F1X0_OBJECTS)
	ar cr $@ $^

$(GD32F1X0_FWL_ROOT)/%.o: $(GD32F1X0_FWL_ROOT)/%.c
	$(CC) -c $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(FIRMWARE_BIN): $(FIRMWARE_ELF)
	$(OBJCOPY) -O binary $< $@
	python ../tools/fw_pad.py

clean:
	$(RM) $(FIRMWARE_ELF) $(FIRMWARE_BIN) $(GD32F1X0_OBJECTS) $(GD32F1X0_FWL_ROOT)/libgd32f1x0_fwl.a

distclean: clean
	$(MAKE) -C $(LIBOPENCM3_ROOT) clean

.PHONY: all clean distclean
