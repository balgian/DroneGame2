//
// Created by Gian Marco Balia
//
// src/keyboard_manager.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <ncurses.h>
#include <termios.h>

FILE *logfile;

void signal_triggered(int signum);

int main(const int argc, char *argv[]) {
    /*
     * Keyboard process
     * @param argv[1]: Write file descriptors
    */
    // * Signal handler
    struct sigaction sa1;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = signal_triggered;
    sa1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <write_fd> <logfile_fd>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int write_fd = atoi(argv[1]);
    if (write_fd <= 0) {
        fprintf(stderr, "Invalid write file descriptor: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // * Parse logfile file descriptors
    int logfile_fd = atoi(argv[argc - 1]);
    logfile = fdopen(logfile_fd, "a");
    if (!logfile) {
        perror("fdopen logfile");
        return EXIT_FAILURE;
    }
    while(1){
        char c = getch();
        switch (c) {
            case 'w': // * Up Left
            case 'e': // * Up
            case 'r': // * Up Right or Reset
            case 's': // * Left or Suspend
            case 'd': // * Brake
            case 'f': // * Right
            case 'x': // * Down Left
            case 'c': // * Down
            case 'v': // * Down Right
            case 'p': // * Pause
            case 'q': {
                // * Quit
                if (write(write_fd, &c, sizeof(c)) == -1) {
                    perror("write");
                    return EXIT_FAILURE;
                }
                break;
            }
            default:
                break;
        }
    }

    close(write_fd);
    return EXIT_SUCCESS;
}

void signal_triggered(int signum) {
    const time_t now = time(NULL);
    const struct tm *t = localtime(&now);
    fprintf(logfile, "[%02d:%02d:%02d] PID: %d - %s\n", t->tm_hour, t->tm_min, t->tm_sec, getpid(),
        "Keyboard manager is active.");
    fflush(logfile);
}