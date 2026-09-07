#include <stdio.h>
#include <string.h>
#include <unistd.h>

void p(char *dst, char *prompt)
{
    char input[4096];
    char *newline;

    puts(prompt);
    read(STDIN_FILENO, input, sizeof(input));

    newline = strchr(input, '\n');
    *newline = '\0';

    strncpy(dst, input, 20);
}

void pp(char *dst)
{
    char first[20];
    char second[20];

    p(first, " - ");
    p(second, " - ");

    strcpy(dst, first);
    dst[strlen(dst)] = ' ';
    dst[strlen(dst) + 1] = '\0';
    strcat(dst, second);
}

int main(void)
{
    char result[42];

    pp(result);
    puts(result);
    return 0;
}
