Processor looks like a GD32F1x0
	DBG_ID register at 0xE004_2000 has value 0x13030410
		REV_ID = 0x1303
		DEV_ID = 0x410

	IRQ 37 for USB interrupts matches table in user manual

PA5 - Adapter signal
	When the adapter is plugged into the cable, it presents a 10 kHz square wave
	to this pin which the bootloader (and app?) sense to detect if the
	bootloader should continute running (signal was present) or the application
	should run (signal not present after a set amount of time)

PB3 - RGB LED (red)
PB4 - RGB LED (blue)
PB5 - RGB LED (green)
	All RGB LED signals are active low - LED is turned on if output is turned
	off.
