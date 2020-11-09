PREFIX ?= arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy

LIBOPENCM3_ROOT := lib/libopencm3
GD32F1X0_FWL_ROOT := lib/GD32F1x0_Firmware_Library_v3.1.0
CMSIS_ROOT := lib/CMSIS

GD32F1X0_FWL_SOURCES := $(wildcard $(GD32F1X0_FWL_ROOT)/Firmware/GD32F1x0_standard_peripheral/Source/*.c)
GD32F1X0_FWL_OBJECTS := $(GD32F1X0_FWL_SOURCES:.c=.o)

CFLAGS := -mthumb -msoft-float -mcpu=cortex-m3
CFLAGS += -std=c11 -g3 -O0
CFLAGS += -I.
CFLAGS += -I$(CMSIS_ROOT)/CMSIS/Core/Include
CFLAGS += -I$(GD32F1X0_FWL_ROOT)/Firmware/CMSIS
CFLAGS += -I$(GD32F1X0_FWL_ROOT)/Firmware/CMSIS/GD/GD32F1x0/Include
CFLAGS += -I$(GD32F1X0_FWL_ROOT)/Firmware/GD32F1x0_standard_peripheral/Include
CFLAGS += -fno-common -ffunction-sections -fdata-sections
CFLAGS += -DGD32F1X0 -DGD32F130_150

LDFLAGS := -L$(LIBOPENCM3_ROOT)/lib
LDFLAGS += -L$(GD32F1X0_FWL_ROOT)
LDFLAGS += --static -nostartfiles
LDFLAGS += -Tlinker.ld

LDLIBS_BASE := -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
LDLIBS_FWL := $(LDLIBS_BASE)
LDLIBS := $(LDLIBS_BASE) -lopencm3_gd32f1x0 -lgd32f1x0_fwl

SOURCES := $(wildcard *.c)

FIRMWARE_ELF := firmware.elf
FIRMWARE_BIN := firmware.bin

# ========
# Firmware
# ========
all: $(FIRMWARE_ELF) $(FIRMWARE_BIN)

$(FIRMWARE_ELF): $(SOURCES) $(LIBOPENCM3_ROOT)/lib/libopencm3_gd32f1x0.a $(GD32F1X0_FWL_ROOT)/libgd32f1x0_fwl.a $(CMSIS_ROOT)
	$(CC) $(CFLAGS) -Wall -Wextra $(LDFLAGS) $(SOURCES) $(LDLIBS) -o firmware.elf

$(FIRMWARE_BIN): $(FIRMWARE_ELF)
	$(OBJCOPY) -O binary $< $@
	python ../tools/fw_pad.py

flash: $(FIRMWARE_BIN)
	python ../tools/flash.py $^

# ==========
# libopencm3
# ==========
$(LIBOPENCM3_ROOT)/Makefile:
	git submodule update --init

$(LIBOPENCM3_ROOT)/lib/libopencm3_%.a: $(LIBOPENCM3_ROOT)/Makefile
	$(MAKE) -C $(LIBOPENCM3_ROOT) TARGETS=gd32/f1x0

# =========================
# GD32F1X0 Firmware Library
# =========================
ifeq ($(wildcard $(GD32F1X0_FWL_ROOT)),)
$(error GD32F1x0 Firmware Library not found: download from http://www.gd32mcu.com/en/download/7?kw=gd32f1x0 and extract into lib/)
endif

$(GD32F1X0_FWL_ROOT)/libgd32f1x0_fwl.a: $(GD32F1X0_FWL_OBJECTS)
	ar cr $@ $^

$(GD32F1X0_FWL_ROOT)/%.o: $(GD32F1X0_FWL_ROOT)/%.c
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LDLIBS_FWL) -c -o $@

# =====
# CMSIS
# =====
$(CMSIS_ROOT):
	git submodule update --init

# =====
# Clean
# =====
clean:
	$(RM) $(FIRMWARE_ELF) $(FIRMWARE_BIN)

distclean: clean
	$(MAKE) -C $(LIBOPENCM3_ROOT) clean
	$(RM) $(GD32F1X0_OBJECTS) $(GD32F1X0_FWL_ROOT)/libgd32f1x0_fwl.a

.PHONY: all clean distclean
