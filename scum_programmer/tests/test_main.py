"""Test module for main.py."""

from click.testing import CliRunner

from scum_programmer.main import main

MAIN_HELP_EXPECTED = """Usage: main [OPTIONS] FIRMWARE

Options:
  -v, --version           Show the version and exit.
  -p, --port TEXT         Serial port to use for nRF. If omitted, detected
                          J-Link ports are listed.
  -b, --baudrate INTEGER  Baudrate to use for nRF.
  -c, --calibrate         Calibrate SCuM after flashing.
  -h, --help              Show this message and exit.
"""


def test_main_help():
    runner = CliRunner()
    result = runner.invoke(main, ["--help"])
    assert result.exit_code == 0
    assert result.output == MAIN_HELP_EXPECTED
