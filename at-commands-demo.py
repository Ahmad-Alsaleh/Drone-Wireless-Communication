import sys

from serial import Serial  # type: ignore[import-untyped]

if "--help" in sys.argv or "-h" in sys.argv:
    print(f"Usage: {sys.argv[0]} (client | server) [PORT]")
    print("connects")
    exit(0)

if len(sys.argv) not in (2, 3):
    print(f"Usage: {sys.argv[0]} (client | server)", file=sys.stderr)
    exit(1)

mode = sys.argv[1]

if len(sys.argv) == 3:
    port = sys.argv[2]
else:
    port = "/dev/cu.usbserial-110" if mode == "client" else "/dev/cu.usbserial-10"

configs = """\
AT+LOG=QUIET
AT+UART=BR, 230400
AT+MODE=TEST
AT+TEST=RFCFG,868,SF7,250,12,15,14,ON,OFF,OFF
"""


def write(msg: str):
    msg = msg.strip() + "\n"
    msg = msg.encode()  # type: ignore[assignment]

    print(f"\n>>> {msg}\n")
    serial.write(msg)


def read():
    while r := serial.read_until():
        print("<<<", r)


serial = Serial(
    port,
    230400,
    timeout=0.5,
)

for line in configs.splitlines():
    write(line)
    read()

if mode == "client":
    # send a simple message as a test
    write('AT+TEST=TXLRPKT, "A4"')
else:
    write("AT+TEST=RXLRPKT")
read()


while True:
    if mode == "client":
        msg = input("\nType Something: ")
        write(msg)
        read()
    else:
        read()
