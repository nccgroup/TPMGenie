# TPM Genie Usage Instructions

TPM Genie has a fairly simple user interface. It exposes a console via the USB interface on the Teensy microcontroller. TPM Genie prints all intercepted TPM traffic to the console in a tabular format. For some commands and responses, TPM Genie will perform some lightweight parsing of the byte stream in order to print a more human readable representation of what was contained within the packet. However, it does help to keep the [TPM command specification](https://trustedcomputinggroup.org/tpm-main-specification/) nearby when using TPM Genie.

## Understanding the Console Output

When TPM Genie starts up, you should first see a banner.

```
***********************************************************************************
****                               TPM Interposer                              ****
****                           Jeremy Boone, NCC Group                         ****
***********************************************************************************
[*] Initializing I2C bus #0 w/ RaspberryPi (slave)
[*] Initializing I2C bus #1 w/ TPM (master)
[*] Starting up in default state: PASSTHROUGH
[*]     You can press 'm' to switch states.
[*] Waiting for data...
```

TPM Genie will sit idle at this point, and wait for traffic to appear on the I2C serial bus. When data is transmitted on the bus, TPM Genie will print the data to the console. What follows is the full transaction for the PCR Read operation performed by the Linux kernel.

```
/-------------------|--------|-------------------------------------------------\
|     Direction     |  reg   | data                                            |
|-------------------|--------|-------------------------------------------------|
| Host > Tnsy       | ACCESS | 00
|        Tnsy > TPM | ACCESS | 00
|        Tnsy < TPM |        | A1
| Host < Tnsy       |        | A1
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 40
| Host < Tnsy       |        | 40
| Host > Tnsy       | BURST  | 02
|        Tnsy > TPM | BURST  | 02
|        Tnsy < TPM |        | 06 00 00
| Host < Tnsy       |        | 06 00 00
| Host > Tnsy       | DATA   | 05 00 C1 00 00 00 0E 00 00 00 15 00 00 00
|        Tnsy > TPM | DATA   | 05 00 C1 00 00 00 0E 00 00 00 15 00 00 00
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 88
| Host < Tnsy       |        | 88
| Host > Tnsy       | DATA   | 05 01
|        Tnsy > TPM | DATA   | 05 01
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 80
| Host < Tnsy       |        | 80
| Host > Tnsy       | STS    | 01 20
|-------------------|--------|--------------------------------
| REQUEST           |        | Command: PcrRead (0x15)
|                   |        | PCR Index: 1
|-------------------|--------|--------------------------------
|        Tnsy > TPM | STS    | 01 20
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 80
| Host < Tnsy       |        | 80
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 90
| Host < Tnsy       |        | 90
| Host > Tnsy       | BURST  | 02
|        Tnsy > TPM | BURST  | 02
|        Tnsy < TPM |        | 1E 00 00
| Host < Tnsy       |        | 1E 00 00
| Host > Tnsy       | DATA   | 05
|        Tnsy > TPM | DATA   | 05
|        Tnsy < TPM |        | 00 C4 00 00 00 1E 00 00 00 00
| Host < Tnsy       |        | 00 C4 00 00 00 1E 00 00 00 00
| Host > Tnsy       | BURST  | 02
|        Tnsy > TPM | BURST  | 02
|        Tnsy < TPM |        | 14 00 00
| Host < Tnsy       |        | 14 00 00
| Host > Tnsy       | DATA   | 05
|        Tnsy > TPM | DATA   | 05
|        Tnsy < TPM |        | 06 81 21 7B BA 51 3A 90 55 D9 66 84 E0 95 30 E0
|                   |        | E9 53 96 C7
|-------------------|--------|--------------------------------
| RESPONSE          |        | Command: PcrRead (0x15)
|                   |        | Out Digest:
|                   |        | 06 81 21 7B BA 51 3A 90 55 D9 66 84 E0 95 30 E0
|                   |        | E9 53 96 C7
|-------------------|--------|--------------------------------
```

### What do the columns mean?

The first column is the **direction** that the packet traveled in. The possible directions are:

- `Host > Tnsy      `: The host is transmitting a command to the TPM, but TPM Genie has intercepted it.
- `       Tnsy > TPM`: TPM Genie is forwarding the intercepted packet on to the TPM.
- `       Tnsy < TPM`: The TPM is sending a response to the previous command, but TPM Genie has intercepted it.
- `Host < Tnsy      `: TPM Genie is sending a potentially modified response back to the host.

The second column is the **register** that the host is accessing on the TPM peripheral. The possible registers are:

- `ACCESS` (`0x00`): The access register is used to gain access to a particular locality.
- `STS` (`0x01`): Reading the status register indicates to the host that the TPM is ready to receive a command, is expecting more data associated with a previous command, or has finished executing a previously issued command and has response data available.
- `BURST` (`0x02`): The host reads the TPM's burst count register to determine how many bytes are available in the data FIFO for either read or write.
- `DATA_FIFO` (`0x05`): The host can write to the TPM's data FIFO register to send a command to the TPM. Likewise, reading this register will retrieve the response payload.
- `VIDDID` (`0x06`): This register returns the unique Vendor ID and Device ID associated with the TPM hardware.

In general, TPM Genie will always pass through `ACCESS`, `STS`, `VIDDID` and `BURST` register values. TPM Genie will never modify read or write operations on these registers because it frankly doesn't need to (although obviously it could be made to do so). There's also some benefit from being as transparent as possible. TPM Genie doesn't need to be fully aware of the TPM protocol, and if it starts changing the `ACCESS` or `STS` register values, it may end up confusing the host's state machine.

TPM Genie does, however, modify the `DATA_FIFO` values, for both read and write operations. More on that in the [TPM serial bus attack demonstrations](docs/Attack_Demonstrations.md) section.

Please note that the **locality** and **register** address are packed into the same byte, each occupying one nibble. In general TPM Genie doesn't need to care much about TPM Locality, because it always manipulates command and response payloads at the same locality level of the host software.

Finally, the **data** column is the actual data that is transmitted over the bus. The first byte is the register that is is being accessed. All following bytes are the payload data.


# In depth packet sequence analysis

Let's analyze the above sequence of packets one logical piece at a time. Whenever the host wishes to issue a command to the TPM, it first checks the `ACCESS` and `STS` registers to check the locality level and to ensure that the TPM is ready to receive a command packet.

```
| Host > Tnsy       | ACCESS | 00
|        Tnsy > TPM | ACCESS | 00
|        Tnsy < TPM |        | A1
| Host < Tnsy       |        | A1
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 40
| Host < Tnsy       |        | 40
```

Next, the host will query the `BURST` register to see how much room is in the `DATA_FIFO`. 

```
| Host > Tnsy       | BURST  | 02
|        Tnsy > TPM | BURST  | 02
|        Tnsy < TPM |        | 06 00 00
| Host < Tnsy       |        | 06 00 00
```

Next, the host can begin writing the command packet to the TPM's `DATA_FIFO` register.

```
| Host > Tnsy       | DATA   | 05 00 C1 00 00 00 0E 00 00 00 15 00 00 00
|        Tnsy > TPM | DATA   | 05 00 C1 00 00 00 0E 00 00 00 15 00 00 00
```

All TPM command headers are 10-bytes in size.

- The first byte (`05`) is the register address for the `DATA_FIFO`.
- The next two bytes are the tag (`00 C1`), and they indicate the type of authorization session in use. For more information on that, consult the whitepaper.
- The next four bytes represents the length of the overall packet (`00 00 00 0E`). In this case, `14`.
- The next four bytes is the command ordinal (`00 00 00 15`), in this case, corresponding to the PCR Read operation.

Notice that there are 3 trailing bytes after the ordinal (`00 00 00`), this is the first three bytes of the command payload. Because we know the overall command size is `14`, there must be one byte missing. It is fairly common to see packets get fragmented like this.

The next four operations are the host checking the TPM's status, and the TPM indicates that it is expecting more bytes as part of the command, which is what we expect because we know the command packet was fragmented.

```
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 88
| Host < Tnsy       |        | 88
```

Then the host writes the final byte of the packet (`01`).

```
| Host > Tnsy       | DATA   | 05 01
|        Tnsy > TPM | DATA   | 05 01
```

And finally, the host tells the TPM that it has finished.

```
| Host > Tnsy       | STS    | 01
|        Tnsy > TPM | STS    | 01
|        Tnsy < TPM |        | 80
| Host < Tnsy       |        | 80
| Host > Tnsy       | STS    | 01 20
```

At this point TPM Genie can tell that the entire packet has been written. Because TPM Genie keeps a copy of all data written to the `DATA_FIFO` register, it can go back and reconstruct the full command packet and it will print out a nice human readable form:

```
|-------------------|--------|--------------------------------
| REQUEST           |        | Command: PcrRead (0x15)
|                   |        | PCR Index: 1
|-------------------|--------|--------------------------------
```

Next, the host will be looking to read the TPM's response packet. We won't analyze each register access in depth, because it behaves pretty much the same way as sending a command. When the entire response header and body have been received, TPM Genie will once again print a human friendly form:

```
|-------------------|--------|--------------------------------
| RESPONSE          |        | Command: PcrRead (0x15)
|                   |        | Out Digest:
|                   |        | 06 81 21 7B BA 51 3A 90 55 D9 66 84 E0 95 30 E0
|                   |        | E9 53 96 C7
|-------------------|--------|--------------------------------
```

## Switching Modes

By pressing the 'm' key in the TPM Genie console, you can cycle between the different modes of operation. Presently, TPM Genie supports four modes:

- **Passthrough**: The default mode of operation. All data is passed through the interposer without modification. Effectively, an attacker can sniff TPM traffic without changing any of it. This in of itself can be an extremely useful tool when analyzing TPM-based systems.
- **Spoof PCR Extend**: In this mode, TPM Genie will modify `TPM_ORD_PCRExtend` request packets to demonstrate that an interposer can control all PCR extend operations.
- **Alter Hardware RNG**: In this mode, the interposer will modify `TPM_ORD_GetRandom` response packets. This demonstrates that an interposer could impair cryptographic operations on the host by controlling the output of the hardware random number generator.
- **Trigger Crash**: This mode is intended to demonstrate one of the many memory corruption vulnerabilities that are described in the research paper. For the sake of simplicity, the command `TPM_ORD_GetRandom` was selected once again. Here, the interposer will modify the “size” field in the response packet in order to trigger a memory corruption bug in the kernel’s response parser.

Each of these modes is explained in the [TPM serial bus attack demonstrations](Attack_Demonstrations.md) section.

## Line Breaks

Pressing the 'enter' key will insert a line break in the TPM Genie console output. This can be useful to insert logical separations between sequences of commands and responses when you're testing your TPM.


