import sys

from serial.tools import list_ports


def get_default_port():
    """Return default serial port."""
    ports = sorted([port for port in list_ports.comports()])
    if sys.platform != "win32":
        ports = [port for port in ports if "J-Link" == port.product]
    if not ports:
        return "/dev/ttyACM0"
    # return first JLink port available
    return ports[0].device
