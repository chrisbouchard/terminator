/* terminator.c
 *
 * Chris Bouchard
 * 21 Nov 2013
 *
 * Run a command as though it were on a terminal using a PTY. This is useful to
 * trick programs into diabling buffering, or otherwise acting as if they are
 * interactive, while still capturing their output.
 *
 * UPDATES:
 * 25 Nov 2013: Input now goes through the PTY as well. [Chris Bouchard]
 *
 */

#define _GNU_SOURCE 1
/* #define ASSERT_DEBUG 1 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stropts.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "my_assert.h"

/* The name the my_assert library will use for printing errors */
#define ASSERT_PROGRAM_NAME "terminator"

/* The number of bytes to read in one go */
#define BUFFER_SIZE BUFSIZ

#define POLL_TIMEOUT 100


struct redirection_info {
    int id;
    int in_fd;
    int out_fd;
    int send_eot;
    int end_all;
};

/* Copy all input from one file descriptor into another. */
static void *redirection_thread_fn(void *arg);


#ifdef __sun
/* Put a terminal into raw mode. This is a library function on most systems,
 * but not Solaris! :D */
static void cfmakeraw(struct termios *termios_p);
#endif


int main(int argc, char **argv) {
    /* The master and slave PTY file descriptors, respectively */
    int fdm, fds;

    /* The exit status with which we will exit. If we make it to the end
     * without changing the value, something went wrong and we should exit with
     * failure. */
    int exitstatus = EXIT_FAILURE;

    /* The child process ID returned by fork */
    pid_t pid;

    /* We need at least one argument, which will be the command to run. */
    ASSERT_WITH_MESSAGE(argc > 1, "Insufficient command line arguments");

    /* Open the PTY multiplexer to get a master PTY. */
    ASSERT_NONNEG(fdm = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK));

    /* Allow a slave PTY to be opened. */
    ASSERT_ZERO(grantpt(fdm));
    ASSERT_ZERO(unlockpt(fdm));

    /* The child process opens a slave PTY. */
    ASSERT_NONNEG(fds = open(ptsname(fdm), O_RDWR | O_NOCTTY));

    /* Fork a child process. */
    ASSERT_NONNEG(pid = fork());

    if (pid == 0) {
        struct termios fds_settings;

        /* System V implementations need STREAMS configuration for the slave
         * PTY. */
        if (isastream(fds)) {
            ASSERT_NONNEG(ioctl(fds, I_PUSH, "ptem"));
            ASSERT_NONNEG(ioctl(fds, I_PUSH, "ldterm"));
        }

        /* Enable raw mode on the slave PTY. */
        ASSERT_ZERO(tcgetattr(fds, &fds_settings));
        cfmakeraw(&fds_settings);
        ASSERT_ZERO(tcsetattr(fds, TCSANOW, &fds_settings));

        ASSERT_NONNEG(setsid());

        /* And duplicates all its I/O to that slave PTY. */
        ASSERT_NONNEG(dup2(fds, STDIN_FILENO));
        ASSERT_NONNEG(dup2(fds, STDOUT_FILENO));
        ASSERT_NONNEG(dup2(fds, STDERR_FILENO));

        ASSERT_ZERO(close(fdm));
        ASSERT_ZERO(close(fds));

        ASSERT_NONNEG(ioctl(0, TIOCSCTTY, 0));

        /* Then it runs the specified command, passing all command line
         * arguments. */
        ASSERT_ZERO(execvp(argv[1], argv + 1));
    }
    else{
        /* The status of the child process returned by waitpid */
        int status;

        pthread_t reader_thread, writer_thread;

        struct redirection_info reader_info = { 0, STDIN_FILENO, fdm, 1, 0 };
        struct redirection_info writer_info = { 1, fdm, STDOUT_FILENO, 0, 1 };

        void *reader_status, *writer_status;

        pthread_create(&reader_thread, NULL, &redirection_thread_fn,
                       &reader_info);

        pthread_create(&writer_thread, NULL, &redirection_thread_fn,
                       &writer_info);

        ASSERT_ZERO(close(fds));

        /* Wait for the child process to exit. */
        ASSERT_NONNEG(waitpid(pid, &status, 0));

#ifdef ASSERT_DEBUG
        fscanf(stderr, "Child exited.\n");
#endif

        pthread_join(reader_thread, &reader_status);
        pthread_join(writer_thread, &writer_status);

        /* Check if the child process exited safely, and if so, capture its
         * exit status. */
        if (WIFEXITED(status)) {
            exitstatus = WEXITSTATUS(status);
        }

        /* Close the master PTY. */
        ASSERT_ZERO(close(fdm));
    }

    /* Exit with the exit code gotten from the child process. */
    return exitstatus;
}


static void *redirection_thread_fn(void *arg) {
    static volatile int all_done = 0;

    int keep_going = 1;
    int found_eof = 0;

    struct redirection_info *info = arg;

    struct pollfd poll_fds[2];

    char buffer[BUFFER_SIZE];
    size_t buffer_length = 0;
    size_t buffer_offset = 0;

    poll_fds[0].fd = info->in_fd;
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = info->out_fd;
    poll_fds[1].events = POLLOUT;

    while ((keep_going &= !all_done) || buffer_length) {
        ASSERT_NONNEG(poll(poll_fds, 2, POLL_TIMEOUT));

        if (poll_fds[0].revents & POLLHUP && !(poll_fds[0].revents & POLLIN)) {
#ifdef ASSERT_DEBUG
            fprintf(stderr, "%d: Hangup on fd %d.\n", info->id, poll_fds[0].fd);
#endif
            poll_fds[0].fd = -1;
            found_eof = 1;
        }

        if (poll_fds[1].revents & POLLHUP) {
#ifdef ASSERT_DEBUG
            fprintf(stderr, "%d: Hangup on fd %d.\n", info->id, poll_fds[1].fd);
#endif
            poll_fds[1].fd = -1;
            keep_going = 0;
            buffer_offset = 0;
            buffer_length = 0;
        }

        if (keep_going && !found_eof && poll_fds[0].revents & POLLIN && buffer_length == 0) {
            size_t n_read;

            ASSERT_NONNEG(n_read = read(poll_fds[0].fd, buffer, BUFFER_SIZE));

            buffer_offset = 0;
            buffer_length = n_read;

#ifdef ASSERT_DEBUG
            fprintf(stderr, "%d: Read %d bytes on fd %d.\n", info->id, n_read,
                    poll_fds[0].fd);
#endif

            if (n_read == 0) {
                poll_fds[0].fd = -1;
                found_eof = 1;
            }
        }

        if (poll_fds[1].revents & POLLOUT) {
            if (buffer_length > 0) {
                size_t n_written;

                ASSERT_NONNEG(n_written = write(poll_fds[1].fd, buffer + buffer_offset,
                                                buffer_length));
                ASSERT_ZERO(fsync(poll_fds[1].fd));

                buffer_offset += n_written;
                buffer_length -= n_written;

#ifdef ASSERT_DEBUG
                fprintf(stderr, "%d: Wrote %d bytes on fd %d. %d bytes remain in buffer.\n",
                        info->id, n_written, poll_fds[1].fd, buffer_length);
#endif
            }
            else if (found_eof) {
                char eot_char = 0x04;

                if (info->send_eot && (isastream(poll_fds[1].fd) || isatty(poll_fds[1].fd))) {
                    ASSERT_NONNEG(write(poll_fds[1].fd, &eot_char, 1));
                    ASSERT_ZERO(fsync(poll_fds[1].fd));

#ifdef ASSERT_DEBUG
                    fprintf(stderr, "%d: Wrote EOT on fd %d. %d bytes remain in buffer.\n",
                            info->id, poll_fds[1].fd, buffer_length);
#endif
                }
                else {
                    ASSERT_NONNEG(write(poll_fds[1].fd, &eot_char, 0));
                    ASSERT_ZERO(fsync(poll_fds[1].fd));

#ifdef ASSERT_DEBUG
                    fprintf(stderr, "%d: Wrote 0 bytes on fd %d. %d bytes remain in buffer.\n",
                            info->id, poll_fds[1].fd, buffer_length);
#endif
                }

                keep_going = 0;
            }
        }
    }

    if (info->end_all) {
#ifdef ASSERT_DEBUG
        fprintf(stderr, "%d: And we are all done!\n", info->id);
#endif
        all_done = 1;
    }

#ifdef ASSERT_DEBUG
    fprintf(stderr, "%d: Thread exited.\n", info->id);
#endif

    return NULL;
}


#ifdef __sun
/* Put a terminal into raw mode. This is a library function on most systems,
 * but not Solaris! :D */
static void cfmakeraw(struct termios *termios_p) {
    /* This implementation is taken from the Linux man page for termios(3). */
    termios_p->c_iflag &= ~(IMAXBEL | IGNBRK | BRKINT | PARMRK | ISTRIP |
                            INLCR | IGNCR | ICRNL | IXON);
    termios_p->c_oflag &= ~OPOST;
    termios_p->c_lflag &= ~(ECHO | ECHONL | /* ICANON |*/ ISIG | IEXTEN);
    termios_p->c_cflag &= ~(CSIZE | PARENB);
    termios_p->c_cflag |= CS8;
}
#endif

