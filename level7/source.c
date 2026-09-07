#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct s_entry
{
    int id;
    char *data;
} t_entry;

char c[80];

void m(void)
{
    printf("%s - %d\n", c, (int)time(NULL));
}

int main(int argc, char **argv)
{
    t_entry *first;
    t_entry *second;
    FILE *pass_file;

    (void)argc;
    first = malloc(sizeof(*first));
    first->id = 1;
    first->data = malloc(8);

    second = malloc(sizeof(*second));
    second->id = 2;
    second->data = malloc(8);

    strcpy(first->data, argv[1]);
    strcpy(second->data, argv[2]);

    pass_file = fopen("/home/user/level8/.pass", "r");
    fgets(c, 0x44, pass_file);
    puts("~~");

    return 0;
}
