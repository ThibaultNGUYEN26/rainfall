#include <stdio.h>
#include <stdlib.h>

int m;

void p(char *input)
{
    printf(input);
}

void n(void)
{
    char buffer[512];

    fgets(buffer, sizeof(buffer), stdin);
    p(buffer);

    if (m == 0x01025544)
        system("/bin/cat /home/user/level5/.pass");
}

int main(void)
{
    n();
    return 0;
}
