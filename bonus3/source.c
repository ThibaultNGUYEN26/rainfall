#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char password[132];
    FILE *pass_file;

    pass_file = fopen("/home/user/end/.pass", "r");
    memset(password, 0, sizeof(password));

    if (pass_file == NULL || argc != 2)
        return -1;

    fread(password, 1, 66, pass_file);
    password[65] = '\0';
    password[atoi(argv[1])] = '\0';

    fread(password + 66, 1, 65, pass_file);
    fclose(pass_file);

    if (strcmp(password, argv[1]) == 0)
        execl("/bin/sh", "sh", NULL);
    else
        puts(password + 66);

    return 0;
}
