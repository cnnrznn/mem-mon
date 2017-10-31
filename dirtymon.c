#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

static int (*real__libc_start_main) (int (*main) (int, char**, char**), int argc, char *argv, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void *stack_end);

static void
open_proc_files(FILE **mf, FILE **pmf)
{
    *mf = fopen("/proc/self/maps", "r");
    if (!*mf) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    *pmf = fopen("/proc/self/pagemap", "r");
    if (!*pmf) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
}

static void
close_proc_files(FILE *mf, FILE *pmf)
{
    if (fclose(mf)) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
    if (fclose(pmf)) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
}

static void
clear_soft_dirty(void)
{
    int fd;

    fd = open("/proc/self/clear_refs", O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    if (write(fd, "4", 1) < 1) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    if (close(fd)) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
}

static int
read_range(FILE *mf, long unsigned *start, long unsigned *end)
{
    int rc;

    // 1. fscanf (%lx-%lx)
    rc = fscanf(mf, "%lx-%lx", start, end);

    if (rc == 2)
        return 1;

    else if (rc == EOF)
        return 0;

    else {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
}

static void
print_dirty_ranges(FILE *pmf, long unsigned start, long unsigned end)
{
    // TODO
    fprintf(stderr, "%lx-%lx\n", start, end);
}

static void
dirty_handler(int sig)
{
    FILE *mf, *pmf;
    long unsigned start, end;

    // 0. open maps, pagemap
    open_proc_files(&mf, &pmf);

    // 1. read /proc/self/maps to find range of [heap]
    while (read_range(mf, &start, &end)) {
        print_dirty_ranges(pmf, start, end);
    }
    fprintf(stderr, "\n");

    close_proc_files(mf, pmf);

    // 4. clear soft-dirty bit
    clear_soft_dirty();
}

int __libc_start_main(int (*main) (int, char**, char**), int argc, char *argv, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void *stack_end)
{
    int rc;

    // 1. setup handler on timer (settimer)
    sighandler_t sig = signal(SIGPROF, dirty_handler);
    if (sig == SIG_ERR) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    // 2. schedule for signals to be sent to the application
    struct timeval tv = {
            .tv_sec = 0,
            .tv_usec = 10000,
        };
    struct itimerval itimer = { 
            .it_interval    = tv,
            .it_value       = tv,
        };
    rc = setitimer(ITIMER_PROF, &itimer, NULL);
    if (rc) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    // 2a. (OPTIONAL) clear dirty pages flag

    // 3. link real__libc_start_main()
    real__libc_start_main = dlsym(RTLD_NEXT, "__libc_start_main");
    if (!real__libc_start_main) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    // 4. call real__libc_start_main()
    return real__libc_start_main(main, argc, argv, init, fini, rtld_fini, stack_end);
}
