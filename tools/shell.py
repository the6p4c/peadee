from pdc002 import PDC002Bootloader

def print_status(s):
    if s == PDC002Bootloader.STATUS_SUCCESS:
        print('status: success')
    elif s == PDC002Bootloader.STATUS_ERROR:
        print('status: error')

def shell_main(pdc):
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
                if len(args) not in [1, 2, 3]:
                    print('usage: read address [length] [base]')
                    continue

                address = int(args[0], 16)
                if address < 0 or address > 0xffffffff:
                    print('invalid address')
                    continue

                length = 1
                if len(args) >= 2:
                    length = int(args[1])
                    if length <= 0:
                        print('invalid length')
                        continue

                base = 'x'
                if len(args) >= 3:
                    base = args[2]
                    if base not in ['x', 'log']:
                        print('invalid base')
                        continue

                data = pdc.read_big(address, length)
                if base == 'x':
                    data = ' '.join([f'{b:02x}' for b in data])
                    print(f'address {address:08x} = {data}')
                else:
                    messages = data.replace(b'\xff', b'\x00').decode('ascii').split('\x00')
                    messages = [msg for msg in messages if len(msg) != 0]
                    for message in messages:
                        print(message)
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
                    f.write(pdc.read_big(address, length))
                print('dump complete')
            elif cmd == 'status':
                print_status(pdc.status_request())
            else:
                print('unknown command')

def main():
    with PDC002Bootloader.open() as pdc:
        shell_main(pdc)

if __name__ == '__main__':
    main()
