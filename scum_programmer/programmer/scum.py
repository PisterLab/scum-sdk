import sys
import time
from dataclasses import dataclass
from enum import IntEnum

import serial
from rich import print
from rich.console import Console
from tqdm import tqdm

from .hdlc import hdlc_encode

# Constants
USB_CHUNK_SIZE = 64
CHUNK_SIZE = 1024


class Command(IntEnum):
    """Enum for commands to send to the SCuM."""

    START = 0x01
    CHUNK = 0x02
    BOOT = 0x03
    CALIBRATE = 0x04


@dataclass
class ScumProgrammerSettings:
    """Settings for the Scum Programmer."""

    port: str
    baudrate: int
    calibrate: bool
    firmware: str


class ScumProgrammer:
    """Class to handle the Scum Programmer."""

    def __init__(self, settings: ScumProgrammerSettings):
        """Initialize the Scum Programmer."""
        self.settings = settings
        self.console = Console()
        self.init()

    def _send_command(self, msg: str, command: bytes):
        """Send a command to the SCuM."""
        pos = 0
        _command = hdlc_encode(command)
        while pos < len(_command):
            self.serial.write(_command[pos : pos + USB_CHUNK_SIZE])
            self.serial.flush()
            pos += USB_CHUNK_SIZE
        response = self.serial.read_until().decode().strip()
        if response != "ACK":
            print(f"[bold red]Error: failed to {msg}[/]")
            sys.exit(1)

    def init(self):
        """Initialize the serial port."""
        try:
            self.serial = serial.Serial(
                port=self.settings.port,
                baudrate=self.settings.baudrate,
                timeout=5,
            )
        except serial.SerialException as e:
            print(f"[bold red]{e}[/]")
            sys.exit(1)

    def start(self):
        command = bytearray()
        command += Command.START.to_bytes(1)
        self._send_command("start", command)

    def program(self):
        """Send firmware to nRF."""
        bindata = bytearray(open(self.settings.firmware, "rb").read())
        print(f"[bold]Firmware size:[/]\t   {len(bindata)}B")

        # Split the bindata in chunks of 32 bytes
        chunks = []
        chunks_count = int(len(bindata) / CHUNK_SIZE) + int(
            len(bindata) % CHUNK_SIZE != 0
        )
        for chunk_idx in range(chunks_count):
            if chunk_idx == chunks_count - 1:
                chunk_size = len(bindata) % CHUNK_SIZE
                data = bindata[
                    chunk_idx * CHUNK_SIZE : chunk_idx * CHUNK_SIZE + chunk_size
                ]
                data += b"\x00" * (CHUNK_SIZE - chunk_size)
            else:
                chunk_size = CHUNK_SIZE
                data = bindata[
                    chunk_idx * CHUNK_SIZE : chunk_idx * CHUNK_SIZE + chunk_size
                ]
            chunks.append(data)

        # Send the binary data over UART
        data_size = len(chunks) * CHUNK_SIZE
        progress = tqdm(
            range(0, data_size),
            unit="B",
            unit_scale=False,
            colour="green",
            ncols=100,
        )
        progress.set_description("Loading firmware to RAM")
        for chunk in chunks:
            command = bytearray()
            command += Command.CHUNK.to_bytes(1)
            command += chunk
            self._send_command("load chunk to RAM", command)
            progress.update(CHUNK_SIZE)
        progress.close()

    def boot(self):
        """Boot the SCuM."""
        # Execute 3-wire bus bootloader on nRF
        print("[bold]Booting SCuM...   [/]", end=" ")
        command = bytearray()
        command += Command.BOOT.to_bytes(1)
        self._send_command("boot SCuM", command)
        print("[bold green]:heavy_check_mark:[/]")

    def calibrate(self):
        """Calibrate SCuM."""
        print("[bold]Calibrating...    [/]", end=" ")
        command = bytearray()
        command += Command.CALIBRATE.to_bytes(1)
        self._send_command("calibrate SCuM", command)
        print("[bold green]:heavy_check_mark:[/]")

    def run(self):
        """Run the Scum Programmer."""
        print("[bold green]Starting Scum Programmer[/]")
        start = time.time()
        # Triggers hard reset on SCuM
        self.start()
        # Send firmware to nRF
        self.program()
        # Boot SCuM
        self.boot()
        if self.settings.calibrate:
            # Calibrate SCuM
            self.calibrate()
        # Close serial port
        self.serial.close()
        print(f"[bold green]Done in {time.time() - start:.3f}s[/]")
