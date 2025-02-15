//
// Created by Gian Marco Balia
//
// src/watchdog.c
// watchdog.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include "macros.h"

int main(int argc, char *argv[]) {
    // * Check if the number of argument correspond
    if (argc != NUM_CHILD_PROCESSES + 1) {
        fprintf(stderr, "Usage: %s <child_pid_1> ... <child_pid_%d> <blackboard_pid> <logfile_fd>\n",
                argv[0], NUM_CHILD_PROCESSES - 2);
        exit(EXIT_FAILURE);
    }
    // * Parse dei PID dei processi figli
    const int num_child_pids = NUM_CHILD_PROCESSES - 2;
    pid_t child_pids[num_child_pids];
    for (int i = 0; i < num_child_pids; i++) {
        child_pids[i] = (pid_t) atoi(argv[i + 1]);
        if (child_pids[i] <= 0) {
            fprintf(stderr, "Invalid child PID: %s\n", argv[i + 1]);
            exit(EXIT_FAILURE);
        }
    }
    // * Parse del PID del processo blackboard
    pid_t blackboard_pid = (pid_t) atoi(argv[num_child_pids + 1]);
    if (blackboard_pid <= 0) {
        fprintf(stderr, "Invalid blackboard PID: %s\n", argv[num_child_pids + 1]);
        exit(EXIT_FAILURE);
    }
    // * Parse del file descriptor del logfile e apertura del file stream
    int logfile_fd = atoi(argv[num_child_pids + 2]);
    FILE *logfile = fdopen(logfile_fd, "r");
    if (!logfile) {
        perror("fdopen logfile");
        exit(EXIT_FAILURE);
    }
    // * Initialise the timestamp for each process
    time_t last_active_time_p[num_child_pids];
    for (int i = 0; i < num_child_pids; i++) {
        last_active_time_p[i] = time(NULL);
    }
    time_t last_active_time_balckboard = time(NULL);
    int dt = 10;
    while (1) {
        // * Sleep before to repeat the cycle
        sleep(3);
        // * Send the signals
        for (int i = 0; i < num_child_pids; i++) {
            if (kill(child_pids[i], SIGUSR1) == 0) {
                last_active_time_p[i] = time(NULL);
            }
        }
        if (kill(blackboard_pid, SIGUSR1) == 0) {
            last_active_time_balckboard = time(NULL);
        }
        // * Verify the processes' time inactivity
        for (int i = 0; i < num_child_pids; i++) {
            time_t now = time(NULL);
            if (difftime(now, last_active_time_p[i]) > dt) {
                for (int j = 0; j < num_child_pids; j++) {
                    kill(child_pids[j], SIGTERM);
                }
                kill(blackboard_pid, SIGTERM);
                exit(EXIT_FAILURE);
            }
        }
        time_t now = time(NULL);
        if (difftime(now, last_active_time_balckboard) > dt) {
            for (int j = 0; j < num_child_pids; j++) {
                kill(child_pids[j], SIGTERM);
            }
            kill(blackboard_pid, SIGTERM);
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}