MEMORY {
	rom (rx)  : ORIGIN = 0x08002c00, LENGTH = 52K
	ram (rwx) : ORIGIN = 0x20000000, LENGTH = 8K
}

INCLUDE lib/libopencm3/lib/cortex-m-generic.ld
