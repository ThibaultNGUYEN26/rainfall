#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *gets(char *s);

void p(void)
{
    char buffer[76];
    uintptr_t saved_return_address;

    fflush(stdout);
    gets(buffer);

    saved_return_address = (uintptr_t)__builtin_return_address(0);
    if ((saved_return_address & 0xb0000000) == 0xb0000000)
    {
        printf("(%p)\n", (void *)saved_return_address);
        _exit(1);
    }

    puts(buffer);
    strdup(buffer);
}

int main(void)
{
    p();
    return 0;
}
