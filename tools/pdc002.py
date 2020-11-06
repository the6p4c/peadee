from contextlib import contextmanager
from dataclasses import dataclass
import math
import struct
import usb1

@dataclass
class Packet:
    payload_type: int
    payload: bytes

    def _checksum_of(self, data):
        # strongest checksum in the world, howeve,r it is so
        # fragile as to shatter when presented with two opposing
        # bit flips .
        return sum(data) & 0xff

    def to_bytes(self):
        assert len(self.payload) < 0x100

        payload_data = list(self.payload)
        payload_data += [0] * (52 - len(payload_data))

        assert len(payload_data) == 52

        b = [
            0xff, 0x55, # magic
            *bytes.fromhex('162ededc2e13'), # fake timestamp
            self.payload_type,
            len(self.payload),
            *payload_data,
            0x00, 0x00 # checksums to be filled later
        ]

        assert len(b) == 64

        b[62] = self._checksum_of(b[8:62])
        b[63] = self._checksum_of(b[0:62])

        return bytes(b)

    def from_bytes(b):
        payload_type = b[8]

        payload_len = b[9]
        payload = b[10:(10 + payload_len)]

        return Packet(
            payload_type=payload_type,
            payload=payload
        )

class PDC002Bootloader:
    VID = 0x0716
    PID = 0x5036

    TIMEOUT = 1000

    STATUS_ERROR = 0x01
    STATUS_SUCCESS = 0x02
    STATUS_REQUEST = 0x03
    FLASH_LOCK = 0x04
    FLASH_UNLOCK = 0x05
    ERASE = 0x08
    PROG = 0x09
    READ_SMALL = 0x0A
    READ_BIG = 0x0B
    RESET = 0x17

    def __init__(self, handle):
        self.handle = handle

    @contextmanager
    def open(vid=VID, pid=PID):
        with usb1.USBContext() as context:
            handle = context.openByVendorIDAndProductID(
                vid, pid,
                skip_on_error=True
            )

            if handle is None:
                raise RuntimeError('could not open PDC002 device')

            with handle.claimInterface(0):
                yield PDC002Bootloader(handle)

    def write_packet(self, p):
        self.handle.interruptWrite(0x01, p.to_bytes())

    def read_packet(self):
        return Packet.from_bytes(
            self.handle.interruptRead(0x81, 64, timeout=self.TIMEOUT)
        )

    def read_status(self):
        p = self.read_packet()
        if p.payload_type in [self.STATUS_ERROR, self.STATUS_SUCCESS]:
            return p.payload_type
        else:
            raise RuntimeError('unexpected status response')

    def status_request(self):
        self.write_packet(Packet(
            payload_type=self.STATUS_REQUEST,
            payload=bytes()
        ))

        return self.read_status()

    def flash_lock(self):
        self.write_packet(Packet(
            payload_type=self.FLASH_LOCK,
            payload=bytes()
        ))

        return self.read_status()

    def flash_unlock(self):
        self.write_packet(Packet(
            payload_type=self.FLASH_UNLOCK,
            payload=bytes()
        ))

        return self.read_status()

    def erase(self, block):
        self.write_packet(Packet(
            payload_type=self.ERASE,
            payload=bytes([0x00, block, 0x00, 0x08])
        ))

    def prog(self, address, data):
        header = struct.pack('<IB', address, len(data))

        self.write_packet(Packet(
            payload_type=self.PROG,
            payload=header + bytes(data)
        ))

    def read_small(self, address, count):
        if count < 1 or count > 64:
            raise RuntimeError('READ_SMALL count must be between 1 and 64 bytes')

        self.write_packet(Packet(
            payload_type=self.READ_SMALL,
            payload=struct.pack('<IB', address, count)
        ))

        p = self.read_packet()
        if p.payload_type != self.READ_SMALL:
            raise RuntimeError('did not get READ_SMALL in reply to READ_SMALL')

        return p.payload

    def read_big(self, address, count):
        if count < 1 or count > 65535:
            raise RuntimeError('READ_BIG count must be between 1 and 65535 bytes')

        self.write_packet(Packet(
            payload_type=self.READ_BIG,
            payload=struct.pack('<IB', address, count)
        ))

        data = []

        num_replies = math.ceil(count / 0x28)
        for i in range(num_replies):
            p = self.read_packet()
            if p.payload_type != self.READ_BIG:
                raise RuntimeError('did not get READ_BIG in reply to READ_BIG')

            data.extend(p.payload)

        return bytes(data[:count])

    def reset(self):
        self.write_packet(Packet(
            payload_type=self.RESET,
            payload=bytes()
        ))

        return self.read_status()
