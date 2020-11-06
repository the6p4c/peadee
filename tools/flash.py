import argparse
import os
from pdc002 import PDC002Bootloader

def parse_args():
    parser = argparse.ArgumentParser(description='PDC002 flashing tool')
    parser.add_argument('firmware', type=argparse.FileType('rb'))

    return parser.parse_args()

def main():
    args = parse_args()

    with PDC002Bootloader.open() as pdc:
        print('checking status...')
        if pdc.status_request() != PDC002Bootloader.STATUS_SUCCESS:
            print('abort: status check failed')
            return

        print('unlocking flash...')
        if pdc.flash_unlock() != PDC002Bootloader.STATUS_SUCCESS:
            print('abort: unlocking flash failed')
            return

        print('erasing...')
        for block in range(0x2c, 0xf8 + 1):
            pdc.erase(block)

        print('locking flash...')
        if pdc.flash_lock() != PDC002Bootloader.STATUS_SUCCESS:
            print('abort: locking flash failed')
            return

        print('unlocking flash...')
        if pdc.flash_unlock() != PDC002Bootloader.STATUS_SUCCESS:
            print('abort: unlocking flash failed')
            return

        print('programming...')
        address = 0x0800_2c00
        while True:
            length = 0x400
            block_data = args.firmware.read(length)

            if len(block_data) == 0:
                break

            if len(block_data) != length:
                print('abort: read block of length != 0x400 from firmware file')
                return

            i = 0
            while length > 0:
                length_to_prog = min(length, 0x28)

                assert address >= 0x0800_2c00
                assert address + length_to_prog <= 0x0800_fc00
                pdc.prog(address, block_data[i:(i + length_to_prog)])

                address += length_to_prog
                i += length_to_prog
                length -= length_to_prog

        print('verifying...')
        args.firmware.seek(0, os.SEEK_SET)

        address = 0x0800_2c00
        while True:
            length = 0x40
            block_data = args.firmware.read(length)

            if len(block_data) == 0:
                break

            if len(block_data) != length:
                print('abort: read block of length != 0x40 from firmware file')
                return

            d = pdc.read_big(address, length)
            if d != block_data:
                print(f'abort: verification failed for block of length 0x{length:x} at address 0x{address:08x}')
                print(d)
                print(block_data)

            address += length

        print('resetting...')
        if pdc.reset() != PDC002Bootloader.STATUS_SUCCESS:
            print('abort: reset failed')
            return

        print('programming complete!')

if __name__ == '__main__':
    main()
