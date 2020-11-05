Payload type field is more like a command, dictates how the payload is to be interpreted
	0x01 STATUS_ERROR
		Device -> host (IN)
	0x02 STATUS_SUCCESS
		Device -> host (IN)
	0x03 STATUS_REQUEST
		Host -> device (OUT)
		Returns a STATUS_ERROR or STATUS_SUCCESS
	0x04 FLASH_LOCK
		Host -> device (OUT)
		Returns a STATUS_SUCCESS
	0x05 FLASH_UNLOCK
		Host -> device (OUT)
		Returns a STATUS_SUCCESS
	0x08 ERASE
		Host -> device (OUT)
		Payload is 4 bytes
			00 XX 00 08
			XX starts at 2c, goes to f8
	0x09 PROG
		Host -> device (OUT)
		Payload is header followed by data
		Header is [start address: 4 bytes LE] [len: 1 byte]
	0x0a READ_SMALL
		Must be completed in a single read - length must not be more than 0x40 hex = 64 dec bytes
		Host -> device (OUT)
			[start address: 4 bytes LE] [len: 1 byte]
		Device -> host (IN)
			Pure data
	0x0b READ_BIG
		Host -> device (OUT)
			[start address: 4 bytes LE] [len: 2 bytes LE]
		Device -> host (IN)
			Pure data
	0x17 RESET
		Host -> device (OUT)
		Returns a STATUS_SUCCESS before resetting

Init sequence
	STATUS_REQUEST -> device
	host <- STATUS_SUCCESS
	READ_SMALL REQUEST -> device
		Reads 15 bytes from 0800_3800
	host <- READ_SMALL REPLY

Flashing sequence
	STATUS_REQUEST -> device
	host <- STATUS_SUCCESS
	FLASH_UNLOCK -> device
	host <- STATUS_SUCCESS

	# Repeat for each block from 0x2c to 0xf8
		ERASE -> device

	FLASH_LOCK -> device
	host <- STATUS_SUCCESS
	FLASH_UNLOCK -> device
	host <- STATUS_SUCCESS

	# Repeat for each block of, at max, 0x28 hex = 40 dec bytes
		PROG -> device

	FLASH_LOCK -> device
	host <- STATUS_SUCCESS

	# Repeat for blocks of 0x400 hex = 1024 dec bytes to verify entire image
		READ_BIG -> device
		# Repeat to read all subblocks of 0x28 hex = 40 dec bytes
			host <- READ_BIG

	RESET -> device
	host <- STATUS_SUCCESS
