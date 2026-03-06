#!/usr/bin/env python

"""SCuM Programmer CLI."""

import click

try:
    from scum_programmer.programmer import __version__
    from scum_programmer.programmer.scum import ScumProgrammer, ScumProgrammerSettings
    from scum_programmer.programmer.serial import get_jlink_ports
except ImportError:
    from programmer import __version__
    from programmer.scum import ScumProgrammer, ScumProgrammerSettings
    from programmer.serial import get_jlink_ports


SERIAL_BAUDRATE_DEFAULT = 460800


@click.command(context_settings=dict(help_option_names=["-h", "--help"]))
@click.version_option(__version__, "-v", "--version", prog_name="scum-programmer")
@click.option(
    "-p",
    "--port",
    default=None,
    help="Serial port to use for nRF. If omitted, detected J-Link ports are listed.",
)
@click.option(
    "-b",
    "--baudrate",
    default=SERIAL_BAUDRATE_DEFAULT,
    help="Baudrate to use for nRF.",
)
@click.option(
    "-c",
    "--calibrate",
    is_flag=True,
    default=False,
    help="Calibrate SCuM after flashing.",
)
@click.argument("firmware", type=click.File(mode="rb"), required=True)
def main(port, baudrate, calibrate, firmware):
    if port is not None:
        ports_to_use = [port]
    else:
        jlink_ports = get_jlink_ports()
        if not jlink_ports:
            click.echo("No J-Link ports detected. Use -p to specify a port.")
            raise click.Abort()
        elif len(jlink_ports) == 1:
            ports_to_use = jlink_ports
        else:
            click.echo("Detected J-Link ports:")
            for i, p in enumerate(jlink_ports, 1):
                click.echo(f"  [{i}] {p}")
            selection = click.prompt(
                "Enter port numbers to program (comma-separated, or 'all')",
                default="all",
            )
            if selection.strip().lower() == "all":
                ports_to_use = jlink_ports
            else:
                try:
                    indices = [int(s.strip()) for s in selection.split(",")]
                    ports_to_use = [jlink_ports[i - 1] for i in indices]
                except (ValueError, IndexError):
                    click.echo("Invalid selection.")
                    raise click.Abort()

    for p in ports_to_use:
        if len(ports_to_use) > 1:
            click.echo(f"\n--- Programming {p} ---")
        programmer_settings = ScumProgrammerSettings(
            port=p,
            baudrate=baudrate,
            calibrate=calibrate,
            firmware=firmware.name,
        )
        programmer = ScumProgrammer(programmer_settings)
        programmer.run()


if __name__ == "__main__":
    main()
