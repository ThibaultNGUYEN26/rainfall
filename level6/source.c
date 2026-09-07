#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef void (*function_pointer)(void);

void n(void)
{
    system("/bin/cat /home/user/level7/.pass");
}

void m(void)
{
    puts("Nope");
}

int main(int argc, char **argv)
{
    char *buffer;
    function_pointer *callback;

    (void)argc;
    buffer = malloc(64);
    callback = malloc(sizeof(*callback));
    *callback = m;

    strcpy(buffer, argv[1]);
    (*callback)();

    return 0;
}
