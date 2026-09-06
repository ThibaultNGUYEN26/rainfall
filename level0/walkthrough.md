# Level 0

## Connect to the virtual machine with SSH

Start the RainFall virtual machine and make sure its SSH port is forwarded to
port `2223` on the host machine. Then connect from a terminal on the host:

```bash
ssh level0@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

Leave the SSH session or open a second terminal on the host. From the root of
this repository, copy the binary with `scp`:

```bash
scp -P 2223 level0@127.0.0.1:/home/user/level0/level0 ./level0/
```

Enter the `level0` password when prompted. The extracted binary will then be
available locally as `level0/level0`.

## Copy the `main` disassembly to the host

`scp` can only copy a file, so first export the GDB output inside the VM. While
connected as `level0`, open the binary with GDB:

```bash
gdb ./level0
```

Then enter these commands in GDB:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level0_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

- `set pagination off` prevents GDB from pausing the output.
- `set logging file` selects the output file.
- `set logging overwrite on` replaces an older dump instead of appending to it.
- `set logging on` records the disassembly until logging is disabled.

Then run this command from the repository root on the host machine:

```bash
scp -P 2223 level0@127.0.0.1:/tmp/level0_main_dump_gdb ./level0/
```

The result is saved locally as `level0/level0_main_dump_gdb`.

Since the binary is already on the host, you can alternatively generate the
file locally without using `scp`:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  ./level0/level0 > ./level0/level0_main_dump_gdb
```

## Analyze the GDB dump

The relevant part of `main` is:

```asm
0x08048ed4 <+20>: call   0x8049710 <atoi>
0x08048ed9 <+25>: cmp    eax,0x1a7
0x08048ede <+30>: jne    0x8048f58 <main+152>
...
0x08048f21 <+97>: call   0x8054700 <setresgid>
...
0x08048f3d <+125>: call   0x8054690 <setresuid>
...
0x08048f4a <+138>: mov    DWORD PTR [esp],0x80c5348
0x08048f51 <+145>: call   0x8054640 <execv>
```

The program reads `argv[1]`, converts it with `atoi`, and compares the result
with `0x1a7`.

Convert the value to decimal:

```gdb
p/d 0x1a7
```

Result:

```text
$1 = 423
```

The string passed to `execv` is stored at `0x80c5348`:

```gdb
x/s 0x80c5348
```

Result:

```text
0x80c5348: "/bin/sh"
```

So the binary does this:

1. Check that the first argument is `423`.
2. Switch to its effective UID/GID with `setresuid` and `setresgid`.
3. Execute `/bin/sh`.

Because the binary is setuid and owned by `level1`, the shell becomes a
`level1` shell when the correct argument is used.

## Exploitation

Run the binary directly in the VM, not from inside GDB. Debuggers usually
disable or interfere with setuid privilege changes.

```bash
/home/user/level0/level0 423
```

A shell opens. Check the identity:

```bash
whoami
```

Read the next password:

```bash
cat /home/user/level1/.pass
```

## Flag

```text
1fe8a524fa4bec01ca4ea2a869af2a02260d4a7d5fe7e7c24d8617e6dca12d3a
```
