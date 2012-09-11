This program opens TCP port 4321 and allows the remote control of which
MX28 host interface is enabled. This is intended for use by the PTS to
perform DPPM testing.

The pl16cfg program accepts the following commands from the host. Note
that all commands are followed by a new line.
	- "mode": This command requests the current mode of the
	MX28. There are 5 possible responses. Each reponse is followed
	by a newline:
		- "DISABLED": indicates that neither SPI or UART are
		enabled.

		- "SPI": indicates that SPI is enabled.

		- "UART": indicates that UART is enabled.

		- "RESET_NEEDED": indicates that something went wrong
		and the MX28 should be reset.

		- "UNKNOWN": currently has no meaning and should never
		be seen. If seen, MX28 should be reset.

	- "spi": This command disables UART mode if it is currently
	enabled, and then enables SPI mode. No response is sent. The
	host should send the mode command to confirm that the new mode
	was successfully enabled. If the MX28 is already in SPI mode,
	nothing is done.

	- "uart": This command disables SPI mode if it is currently
	enabled, and then enables UART mode. No response is sent. The
	host should send the mode command to confirm that the new mode
	was successfully enabled. If the MX28 is already in UART mode,
	nothing is done.

	- "disable": This command disables UART mode or SPI mode
	(whichever is enabled). No response is sent. The host should send
	the mode command to confirm that the new mode was successfully
	enabled. If the MX28 is already in disabled mode, nothing is done.

After requesting a new mode (ie. spi) the "mode" command can be
immediately sent to the MX28. When the MX28 is in the process of changing
modes, commands are queued and will be processed when the current command
has finished executing. This means that there can be a small delay (less
than 5 seconds) in the response of the "mode" command if the MX28 is busy.
