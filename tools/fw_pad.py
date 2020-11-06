with open('firmware.bin', 'ab') as f:
    l = f.tell()
    f.write(bytes([0xff] * (0xd000 - l - 4)))
    f.write(bytes([0x19, 0x82, 0xAE, 0x00]))
