#include <iostream>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CGROUP_FOLDER "/sys/fs/cgroup/pids/container"
#define concat(a,b) (a"" b)

// allocating stack memory for the process
char* stack_memory() {
    const int stackSize = 65536;
    auto *stack = new (std::nothrow) char[stackSize];

    if (stack == nullptr) {
        printf("cannot allocate memory \n");
        exit(EXIT_FAILURE);
    }

    return stack + stackSize; // here we move pointer to the end of the array because stack grows backward
}

void write_rule(const char *path, const char *value) {
    int fp = open(path, O_WRONLY | O_APPEND);
    write(fp, value, strlen(value));
    close(fp);
}

void limitProcessCreation() {
    mkdir(CGROUP_FOLDER, S_IRUSR | S_IWUSR);
    const char* pid = std::to_string(getpid()).c_str();

    write_rule(concat(CGROUP_FOLDER, "cgroup.procs"), pid);
    write_rule(concat(CGROUP_FOLDER, "notify_on_release"), "1");
    write_rule(concat(CGROUP_FOLDER, "pids.max"),"5");
}

template <typename Function>
void clone_process(Function&& function, int flags) {
    auto pid = clone(function, stack_memory(), flags, 0);
    printf("process with pid %d was created", pid);
    wait(nullptr);
}

void setHostName(const char *name) {
    sethostname(name, strlen(name));
}

void setup_variables() {
    clearenv();
    setenv("TERM", "xterm-256color", 0);
    setenv("PATH", "/bin/:sbin/:/usr/bin:/?usr/sbin", 0);
}

void setup_root(const char *folder) {
    chroot(folder);
    chdir("/");
}

int run(const char *name) {
    char *_args[] = { (char *)name, (char *)0 };
    return execvp(name, _args);
}



// define env for the process
int jail(void *args) {

    printf("child pid: %d\n", getpid());

    limitProcessCreation();

    setHostName("my-container");
    setup_variables();

    setup_root("./root");

    mount("proc", "/proc", "proc", 0, 0);

    auto run_this = [](void *args) ->int { run("/bin/sh"); };

    clone_process(run_this, SIGCHLD);

    umount("/proc");
    return EXIT_SUCCESS;
}

int main() {
    printf("parent process created with pid: %d\n", getpid());

    clone_process(jail, CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD);
    
    wait(nullptr);
    return EXIT_SUCCESS;
}