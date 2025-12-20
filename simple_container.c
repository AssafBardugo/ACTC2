#define _GNU_SOURCE
#include <cap-ng.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sched.h>
#include <seccomp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK_RETVAL(cmd, name)    do { if ((cmd) < 0) {				\
		fprintf(stderr, "call to \"%s\" failed: %s\n", name, strerror(errno));	\
                exit(EXIT_FAILURE); } } while (0);

char *cont_name = "mycontainer";
char *cont_cgroup_dir = "/sys/fs/cgroup/simple_container";
char *memory_max = "100000000\n";


int main(int argc, char *argv[])
{
	char *rootfs_path;
	char *command;

        pid_t child_pid;
        int wstatus;
	int sync_pipe[2];
	char dummy;

	int fd;
	char old_rootfs[PATH_MAX];
	char file_path[PATH_MAX];
	char buf4write[100];
	size_t len;

	capng_type_t cap_set;
	scmp_filter_ctx ctx;


        if (argc < 3) {
                fprintf(stderr, "One or more arguments are missing.\n");
                return 1;
        }
	rootfs_path = argv[1];
	command = argv[2];

	rmdir(cont_cgroup_dir);	// should fail (silently) if cont_cgroup_dir not exists

	CHECK_RETVAL(mkdir(cont_cgroup_dir, 0755), "mkdir - cgroup")

	snprintf(file_path, sizeof(file_path), "%s/memory.max", cont_cgroup_dir);

	CHECK_RETVAL(fd = open(file_path, O_WRONLY), "open - memory.max")

	CHECK_RETVAL(write(fd, memory_max, strlen(memory_max)), "write - memory.max")
	CHECK_RETVAL(close(fd), "close - memory.max")

	CHECK_RETVAL(unshare(CLONE_NEWPID), "unshare - pid")

	CHECK_RETVAL(pipe(sync_pipe), "pipe - sync")

	CHECK_RETVAL(child_pid = fork(), "fork")

        if (child_pid > 0) {
		CHECK_RETVAL(close(sync_pipe[0]), "close - read-end-parent")

		snprintf(file_path, sizeof(file_path), "%s/cgroup.procs", cont_cgroup_dir);
		CHECK_RETVAL(fd = open(file_path, O_WRONLY), "open - cgroup-procs")

		len = snprintf(buf4write, sizeof(buf4write), "%d\n", child_pid);
		CHECK_RETVAL(write(fd, buf4write, len), "write - cgroup-procs")
		CHECK_RETVAL(close(fd), "close - cgroup-procs")

		/* signal child to continue */
    		CHECK_RETVAL(close(sync_pipe[1]), "close - write-end-parent")

                CHECK_RETVAL(wait(&wstatus), "wait")

                if (WIFEXITED(wstatus))
                        fprintf(stdout, "Child exited with status %d\n", WEXITSTATUS(wstatus));

		CHECK_RETVAL(rmdir(cont_cgroup_dir), "rmdir - cgroup-dir")

        } else {
		CHECK_RETVAL(close(sync_pipe[1]), "close - write-end-container")

		/* wait until parent closes pipe */
		CHECK_RETVAL(read(sync_pipe[0], &dummy, 1), "read - sync-read");

		CHECK_RETVAL(close(sync_pipe[0]), "close - read-end-container")

                CHECK_RETVAL(unshare(CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC), "unshare - container")
                CHECK_RETVAL(sethostname(cont_name, strlen(cont_name)), "sethostname")

		CHECK_RETVAL(mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL), "mount - root")
		CHECK_RETVAL(mount(rootfs_path, rootfs_path, NULL, MS_BIND | MS_REC, NULL), "mount - bind")

		snprintf(old_rootfs, sizeof(old_rootfs), "%s/.oldroot", rootfs_path);
        	CHECK_RETVAL(mkdir(old_rootfs, 0755), "mkdir - old-rootfs")

		CHECK_RETVAL(syscall(SYS_pivot_root, rootfs_path, old_rootfs), "pivot_root")

		CHECK_RETVAL(chdir("/"), "chdir");
		mkdir("/proc", 0555);	// ignore EEXIST

		CHECK_RETVAL(mount("proc", "/proc", "proc", 0, NULL), "mount - proc");

        	CHECK_RETVAL(umount2("/.oldroot", MNT_DETACH), "unmount2")
	        CHECK_RETVAL(rmdir("/.oldroot"), "rmdir - old-rootfs")


		/** Security - Capabilities **/
		capng_clear(CAPNG_SELECT_BOTH);
		cap_set = CAPNG_EFFECTIVE | CAPNG_PERMITTED | CAPNG_BOUNDING_SET;

		CHECK_RETVAL(capng_update(CAPNG_ADD, cap_set, CAP_KILL), "capng_update - KILL")
		CHECK_RETVAL(capng_update(CAPNG_ADD, cap_set, CAP_SETGID), "capng_update - SETGID")
		CHECK_RETVAL(capng_update(CAPNG_ADD, cap_set, CAP_SETUID), "capng_update - SETUID")
		CHECK_RETVAL(capng_update(CAPNG_ADD, cap_set, CAP_NET_BIND_SERVICE), "capng_update - SERVICE")
		CHECK_RETVAL(capng_update(CAPNG_ADD, cap_set, CAP_SYS_CHROOT), "capng_update - CHROOT")

		CHECK_RETVAL(capng_apply(CAPNG_SELECT_BOTH), "capng_apply")


		/** Security - Seccomp **/
		ctx = seccomp_init(SCMP_ACT_ALLOW);
		CHECK_RETVAL((ctx != NULL) - 1, "seccomp_init")

		CHECK_RETVAL(seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM),
					     SCMP_SYS(reboot), 0), "seccomp - reboot")
		CHECK_RETVAL(seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM),
					     SCMP_SYS(swapon), 0), "seccomp - swapon")
		CHECK_RETVAL(seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM),
					     SCMP_SYS(swapoff), 0), "seccomp - swapoff")
		CHECK_RETVAL(seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM),
					     SCMP_SYS(init_module), 0), "seccomp - init_module")
		CHECK_RETVAL(seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM),
					     SCMP_SYS(finit_module), 0), "seccomp - finit_module")
		CHECK_RETVAL(seccomp_rule_add(ctx, SCMP_ACT_ERRNO(EPERM),
					     SCMP_SYS(delete_module), 0), "seccomp - delete_module")

		CHECK_RETVAL(seccomp_load(ctx), "seccomp_load")
		seccomp_release(ctx);

		/* run container */
                execvp(command, &argv[2]);

                CHECK_RETVAL(-1, "execvp")	// we should never be here
        }
        return 0;
}

