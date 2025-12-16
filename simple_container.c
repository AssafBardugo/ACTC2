#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>

#define CHECK_SYSCALL(cmd, name)    if ((cmd) == -1) {  \
                printf("%s() fail. errno=%d\n", name, errno);   \
                exit(1); }

char* cont_name = "mycontainer";


int main(int argc, const char* argv[]) {
    char* rootfs_path;
    char* command;
    pid_t child_pid;
    int wstatus;
    char old_rootfs[PATH_MAX];

    if (argc < 3) {
        printf("One or more arguments are missing.\n");
        return 1;
    }
    rootfs_path = argv[1];
    command = argv[2];

    CHECK_SYSCALL(unshare(CLONE_NEWPID), "unshare-parent")

    CHECK_SYSCALL(child_pid = fork(), "fork")

    if (child_pid > 0) {

        CHECK_SYSCALL(wait(&wstatus), "wait")

        if (WIFEXITED(wstatus)) {
            printf("Child exited with status %d\n", WEXITSTATUS(wstatus));
        }
    } else {
        CHECK_SYSCALL(unshare(CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC), "unshare-child")
        CHECK_SYSCALL(sethostname(cont_name, strlen(cont_name)), "sethostname")

        CHECK_SYSCALL(mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL), "mount")
        CHECK_SYSCALL(mount(rootfs_path, rootfs_path, NULL, MS_BIND | MS_REC, NULL), "mount-bind")

        snprintf(old_rootfs, sizeof(old_rootfs), "%s/.oldroot", rootfs_path);
        CHECK_SYSCALL(mkdir(old_rootfs, 0755), "mkdir")

        CHECK_SYSCALL(syscall(SYS_pivot_root, rootfs_path, old_rootfs), "pivot_root")

        CHECK_SYSCALL(mount("proc", "/proc", "proc", 0, NULL), "mount-proc")

        CHECK_SYSCALL(chdir("/"), "chdir")
        CHECK_SYSCALL(umount2("/.oldroot", MNT_DETACH), "unmount2")
        CHECK_SYSCALL(rmdir("/.oldroot"), "rmdir")


        execvp(command, argv + 2);
        // we should never be here
        CHECK_SYSCALL(-1, "execvp")
    }
    return 0;
}
