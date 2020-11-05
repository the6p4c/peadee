from dataclasses import dataclass
import math
import struct
import usb1
from pprint import pprint

PDC002_VID = 0x0716
PDC002_PID = 0x5036

FAKE_TIMESTAMP = bytes.fromhex('162ededc2e13')

@dataclass
class Packet:
    timestamp: bytes
    payload_type: int
    payload: bytes

    def _checksum_of(self, data):
        return sum(data) & 0xff

    def to_bytes(self):
        assert len(self.timestamp) == 6
        assert len(self.payload) < 0x100

        payload_data = list(self.payload)
        payload_data += [0] * (52 - len(payload_data))

        assert len(payload_data) == 52

        b = [
            0xff, 0x55, # magic
            *self.timestamp,
            self.payload_type,
            len(self.payload),
            *payload_data,
            0x00, 0x00
        ]

        assert len(b) == 64

        b[62] = self._checksum_of(b[8:62])
        b[63] = self._checksum_of(b[0:62])

        return bytes(b)

    def from_bytes(b):
        timestamp = b[2:8]
        payload_type = b[8]

        payload_len = b[9]
        payload = b[10:(10 + payload_len)]

        return Packet(
            timestamp=timestamp,
            payload_type=payload_type,
            payload=payload
        )

def packet_write(handle, p):
    handle.interruptWrite(0x01, p.to_bytes())

def packet_read(handle):
    return Packet.from_bytes(handle.interruptRead(0x81, 64))

def read_big(handle, address, length):
    p = Packet(
        timestamp=FAKE_TIMESTAMP,
        payload_type=0x0b,
        payload=struct.pack('<IH', address, length)
    )
    packet_write(handle, p)

    data = []

    num_replies = math.ceil(length / 0x28)
    for i in range(num_replies):
        p = packet_read(handle)
        data.extend(p.payload)

    return data[:length]

def shell_main(handle):
    running = True
    while running:
        full_cmd = input('> ')
        full_cmd = full_cmd.split('#')[0]

        cmds = full_cmd.split(';')
        for cmd in cmds:
            cmd = cmd.strip().split(' ')

            if len(cmd) == 0:
                continue

            cmd, args = cmd[0], cmd[1:]

            if len(cmd) == 0:
                continue

            if cmd == 'q' or cmd == 'quit' or cmd == 'exit':
                running = False
                break
            elif cmd == 'r' or cmd == 'read':
                if len(args) not in [1, 2]:
                    print('usage: read address [length]')
                    continue

                address = int(args[0], 16)
                if address < 0 or address > 0xffffffff:
                    print('invalid address')
                    continue

                length = 1
                if len(args) == 2:
                    length = int(args[1])
                    if length <= 0:
                        print('invalid length')
                        continue

                data = read_big(handle, address, length)
                data = ' '.join([f'{b:02x}' for b in data])
                print(f'address {address:08x} = {data}')
            elif cmd == 'd' or cmd == 'dump':
                if len(args) != 3:
                    print('usage: dump address length filename')
                    continue

                address = int(args[0], 16)
                if address < 0 or address > 0xffffffff:
                    print('invalid address')
                    continue

                length = int(args[1])
                if length <= 0:
                    print('invalid length')
                    continue

                filename = args[2]
                print(f'dumping to {filename}')
                with open(filename, 'wb') as f:
                    f.write(bytes(read_big(handle, address, length)))
                print('dump complete')
            else:
                print('unknown command')

def main():
    with usb1.USBContext() as context:
        handle = context.openByVendorIDAndProductID(
            PDC002_VID, PDC002_PID,
            skip_on_error=True
        )

        if handle is None:
            print('could not open device')
            return

        print('opened device')

        with handle.claimInterface(0):
            print('claimed interface')
            shell_main(handle)

if __name__ == '__main__':
    main()
