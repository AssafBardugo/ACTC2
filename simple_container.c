#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sched.h>
#include <errno.h>

#define CHECK_SYSCALL(cmd, name)    \
            if ((cmd) == -1) {  \
                printf("%s() fail. errno=%d\n", name, errno);   \
                exit(1); }

#define UNSHARE_FLAGS   CLONE_NEWUTS|CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWPID
char* cont_name = "mycontainer";

int main(int argc, const char* argv[]) {
    char* rootfs_path;
    pid_t child_pid;
    int wstatus;

    if (argc < 3) {
        printf("One or more arguments are missing.\n");
        return 1;
    }
    rootfs_path = argv[1];

    CHECK_SYSCALL(child_pid = fork(), fork)

    if (child_pid > 0) {

        CHECK_SYSCALL(wait(&wstatus), wait)

        if (WIFEXITED(wstatus)) {
            printf("Child exited with status %d\n", WEXITSTATUS(wstatus));
        }
    } else {
        CHECK_SYSCALL(unshare(UNSHARE_FLAGS), unshare)
        CHECK_SYSCALL(sethostname(cont_name, strlen(cont_name)), sethostname)
        execvp(argv[2], argv + 2);
        // we should never be here
        CHECK_SYSCALL(-1, execvp)
    }
    return 0;
}
