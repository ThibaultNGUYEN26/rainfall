#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct s_user
{
    char data[76];
} t_user;

int language;

void greetuser(t_user user)
{
    char greeting[72];

    if (language == 1)
        strcpy(greeting, "Hyvaa paivaa ");
    else if (language == 2)
        strcpy(greeting, "Goedemiddag! ");
    else
        strcpy(greeting, "Hello ");

    strcat(greeting, user.data);
    puts(greeting);
}

int main(int argc, char **argv)
{
    t_user user;
    char *lang;

    if (argc != 3)
        return 1;

    memset(&user, 0, sizeof(user));
    strncpy(user.data, argv[1], 40);
    strncpy(user.data + 40, argv[2], 32);

    lang = getenv("LANG");
    if (lang != NULL)
    {
        if (memcmp(lang, "fi", 2) == 0)
            language = 1;
        else if (memcmp(lang, "nl", 2) == 0)
            language = 2;
    }

    greetuser(user);
    return 0;
}
