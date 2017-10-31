#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

static struct itimerval itimer;

static int (*real__libc_start_main) (int (*main) (int, char**, char**), int argc, char *argv, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void *stack_end);

static void
open_proc_files(int *mfd, int *pmfd)
{
    *mfd = open("/proc/self/maps", O_RDONLY);
    if (*mfd < 0) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    *pmfd = open("/proc/self/pagemap", O_RDONLY);
    if (*pmfd < 0) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
}

static void
close_proc_files(int mfd, int pmfd)
{
    if (close(mfd)) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
    if (close(pmfd)) {
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
read_range(int mfd, long unsigned *start, long unsigned *end)
{
    int rc, i;
    char line[256];

    i = 0;
    while ((rc = read(mfd, line+i, 1)) == 1) {
        if (line[i] == '\n') {
            line[i] = '\0';
            break;
        }
        i++;
    }

    if (rc == 0) {
        return 0;
    }
    else if (rc < 0) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    rc = sscanf(line, "%lx-%lx", start, end);
    if (rc < 2) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    return 1;
}

static void
print_dirty_ranges(int pmfd, long unsigned start, long unsigned end)
{
    off_t pos, pos_end;
    const long unsigned page_size = getpagesize();
    long pinfo;
    int rc;

    pos = start / page_size;
    pos_end = end / page_size;

    // 1. seek to start/page_size
    pos = lseek(pmfd, pos, SEEK_SET);
    if (pos == (off_t)-1) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    // 2. read 64 bytes for every page in region
    while (pos < pos_end) {
        rc = read(pmfd, &pinfo, sizeof(long));
        if (rc < sizeof(long)) {
            fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
            exit(1);
        }
        if (pinfo & ((long)1 << 55)) {
        }
    }
}

static void
dirty_handler(int sig)
{
    int mfd, pmfd;
    long unsigned start, end;
    int rc;

    // 0. open maps, pagemap
    open_proc_files(&mfd, &pmfd);

    // 1. read /proc/self/maps to find range of [heap]
    while (read_range(mfd, &start, &end)) {
        fprintf(stderr, "%lx-%lx\n", start, end);
        print_dirty_ranges(pmfd, start, end);
    }
    fprintf(stderr, "\n");

    close_proc_files(mfd, pmfd);

    // 4. clear soft-dirty bit
    clear_soft_dirty();

    // 5. reset timer
    rc = setitimer(ITIMER_VIRTUAL, &itimer, NULL);
    if (rc) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }
}

int __libc_start_main(int (*main) (int, char**, char**), int argc, char *argv, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void *stack_end)
{
    int rc;

    // 1. setup handler on timer (settimer)
    sighandler_t sig = signal(SIGVTALRM, dirty_handler);
    if (sig == SIG_ERR) {
        fprintf(stderr, "(%s:%d)", __FILE__, __LINE__);
        exit(1);
    }

    // 2. schedule for signals to be sent to the application
    struct timeval tv = {
            .tv_sec = 1,
            .tv_usec = 0,
        };
    struct timeval zero = {
            .tv_sec = 0,
            .tv_usec = 0,
        };

    itimer.it_interval    = zero;
    itimer.it_value       = tv;

    rc = setitimer(ITIMER_VIRTUAL, &itimer, NULL);
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
