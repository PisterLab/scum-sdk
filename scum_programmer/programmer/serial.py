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


def get_jlink_ports():
    """Return all detected J-Link serial ports (one per physical board).

    nRF DK boards enumerate two CDC ports per board: the JLink debug interface
    (macOS suffix '1') and the UART bridge to the nRF application (suffix '3').
    When both appear, only the UART bridge port is kept.
    """
    ports = sorted([port for port in list_ports.comports()])
    if sys.platform != "win32":
        ports = [port for port in ports if "J-Link" == port.product]

    # Group by USB serial number; per board keep the higher-numbered device
    # (the UART bridge). Ports without a serial number are included as-is.
    by_serial = {}
    for port in ports:
        sn = port.serial_number
        if sn is None:
            by_serial[port.device] = port
        elif sn not in by_serial or port.device < by_serial[sn].device:
            by_serial[sn] = port

    return [p.device for p in by_serial.values()]
