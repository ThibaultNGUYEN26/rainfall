#include <stdio.h>
#include <stdlib.h>

char *gets(char *s);

void run(void)
{
    fwrite("Good... Wait what?\n", 1, 19, stdout);
    system("/bin/sh");
}

int main(void)
{
    char buffer[64];

    gets(buffer);
    return 0;
}
