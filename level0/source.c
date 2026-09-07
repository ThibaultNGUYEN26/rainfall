#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char *shell_argv[2];
    gid_t gid;
    uid_t uid;

    (void)argc;
    if (atoi(argv[1]) != 423)
    {
        write(STDOUT_FILENO, "No !\n", 5);
        return 0;
    }

    shell_argv[0] = strdup("/bin/sh");
    shell_argv[1] = NULL;

    gid = getegid();
    uid = geteuid();
    setresgid(gid, gid, gid);
    setresuid(uid, uid, uid);

    execv("/bin/sh", shell_argv);
    return 0;
}
