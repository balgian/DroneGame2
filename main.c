//
// Created by Gian Marco Balia
//
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include "macros.h"

FILE *logfile;

void write_log(FILE *logfile, pid_t pid, const char *message);
void signal_triggered(int signum);
int create_pipes(int pipes[NUM_CHILD_PIPES][2]);
int create_processes(int pipes_out[NUM_CHILD_PIPES-1][2], int pipes_in[2],
    pid_t pids[NUM_CHILD_PROCESSES-2], int logfile_fd, pid_t pgid);
pid_t create_watchdog_process(pid_t pgid, int logfile_fd);
pid_t create_blackboard_process(int pipes_in[NUM_CHILD_PIPES-2][2], int pipes_out[2],
    pid_t watchdog_pid, int logfile_fd, pid_t pgid);

int main(void) {
    // * Signal handler
    struct sigaction sa1;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = signal_triggered;
    sa1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // * Set the main process as the leader of a new process group
    pid_t pgid = getpid();  // * The parent's PID is the process group ID
    if (setpgid(0, pgid) == -1) {
        perror("setpgid");
        exit(EXIT_FAILURE);
    }

    // * Create the logfile
    logfile = fopen("./logfile.txt", "w+");
    if (!logfile) {
        perror("Errore apertura logfile");
        exit(EXIT_FAILURE);
    }
    int logfile_fd = fileno(logfile);
    write_log(logfile, getpid(), "Main process started.");

    // * Declaration of pipes and process IDs
    // * Those two are the pipes from Drone and Keyboard to Blackboard
    int pipes[NUM_CHILD_PIPES-1][2];
    int pipe_blackboard[2];
    pid_t pids[NUM_CHILD_PROCESSES]; // * Array to hold child and blackboard PIDs

    // * Step 1: Create pipes
    if (create_pipes(pipes) == -1) {
        fprintf(stderr, "Failed to create pipes.\n");
        exit(EXIT_FAILURE);
    }

    // * Step 2: Create processes that use pipes
    if (create_processes(pipes, pipe_blackboard, pids, logfile_fd, pgid) == -1) {
        fprintf(stderr, "Failed to create processes.\n");
        // * Close all pipes before exiting
        for (int i = 0; i < NUM_CHILD_PIPES; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
        close(pipe_blackboard[0]);
        close(pipe_blackboard[1]);
        exit(EXIT_FAILURE);
    }

    // !!!
    // * Step 3: Create the Watchdog process
    const pid_t watchdog_pid = create_watchdog_process(pgid, logfile_fd);
    if (watchdog_pid == -1) {
        fprintf(stderr, "Failed to create watchdog process.\n");
        // * Terminate already created child processes
        for (int i = 0; i < NUM_CHILD_PROCESSES-1; i++) {
            kill(pids[i], SIGTERM);
        }
        // * Close all pipes before exiting
        for (int i = 0; i < NUM_CHILD_PIPES; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
        close(pipe_blackboard[0]);
        close(pipe_blackboard[1]);
        exit(EXIT_FAILURE);
    }

    // * Step 4: Create the Blackboard Process
    const pid_t blackboard_pid = create_blackboard_process(pipes, pipe_blackboard, watchdog_pid, logfile_fd, pgid);
    if (blackboard_pid == -1) {
        fprintf(stderr, "Failed to create blackboard process.\n");
        // * Terminate child processes and watchdog
        for (int i = 0; i < NUM_CHILD_PROCESSES-1; i++) {
            kill(pids[i], SIGTERM);
        }
        kill(watchdog_pid, SIGTERM);
        // * Close all pipes before exiting
        for (int i = 0; i < NUM_CHILD_PIPES; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
        close(pipe_blackboard[0]);
        close(pipe_blackboard[1]);
        exit(EXIT_FAILURE);
    }

    // ! Step 5: Close All Pipes in the Parent Process
    for (int i = 0; i < NUM_CHILD_PIPES; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    close(pipe_blackboard[0]);
    close(pipe_blackboard[1]);

    // ! Step 6: Wait for All Child Processes to finish
    for (int i = 0; i < NUM_CHILD_PROCESSES; i++) {
        waitpid(pids[i], NULL, 0);
    }
    waitpid(blackboard_pid, NULL, 0);
    waitpid(watchdog_pid, NULL, 0);
    return 0;
}

void write_log(FILE *logfile, pid_t pid, const char *message) {
    const time_t now = time(NULL);
    const struct tm *t = localtime(&now);
    fprintf(logfile, "[%02d:%02d:%02d] PID: %d - %s\n",
            t->tm_hour, t->tm_min, t->tm_sec, pid, message);
    fflush(logfile);
}

void signal_triggered(int signum) {
    write_log(logfile, getpid(), "Main is active.\n");
}

int create_pipes(int pipes[NUM_CHILD_PIPES][2]) {
    /*
    * Function to create NUM_PIPES pipes.
    * @param pipes An array to store the file descriptors of the pipes.
    * @return 0 on success, -1 on failure.
    */
    for (int i = 0; i < NUM_CHILD_PIPES; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            // * Close any previously opened pipes before exiting
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return -1;
        }
    }
    return 0;
}

int create_processes(int pipes_out[NUM_CHILD_PIPES-1][2], int pipes_in[2],
    pid_t pids[NUM_CHILD_PROCESSES-2], const int logfile_fd, const pid_t pgid) {
    /*
     * Function to create child and blackboard processes.
     * @param pipes An array containing the file descriptors of the pipes.
     * @param pids An array to store the PIDs of the child processes.
     * @return 0 on success, -1 on failure.
     */
    // * Array of executable paths corresponding to each child process
    const char *child_executables[NUM_CHILD_PROCESSES-2] = {
        "./keyboard_manager",
        "./obstacles",
        "./targets_generator",
        "./drone_dynamics",
    };
    /*
     * Create 5 processes:
     * - 0: Keyboard input manager (write to Blackboard -> 1 pipe)
     * - 1: Obstacles generator (write to Targets -> 1 pipe)
     * - 2: Targets generator (read from Obstacles -> same pipe of Obstacles)
     * - 3: Drone dynamics process (read & write from/to Blackboard-> 2 pipes)
    */
    for (int i = 0; i < NUM_CHILD_PROCESSES-2; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            // * Cleanup: kill any previously created children
            for (int k = 0; k < i; k++) {
                kill(pids[k], SIGTERM);
            }
            return -1;
        }

        if (pids[i] == 0) {
            // * Join the main process's process group
            if (setpgid(0, pgid) == -1) {
                perror("setpgid in child");
                exit(EXIT_FAILURE);
            }

            // * Join the main process's process group
            if (setpgid(0, pgid) == -1) {
                perror("setpgid in child");
                exit(EXIT_FAILURE);
            }

            // * Prepare the logfile file descriptor to be passet with exec
            char logfile_fd_str[10];
            snprintf(logfile_fd_str, sizeof(logfile_fd_str), "%d", logfile_fd);

            // * If this is child #0 (keyboard_manager), it's write-only. So we close the read end of pipe[i], keep the write end open.
            if (i == 0) {
                // * Close all not needed pipes
                for (int j = 0; j < NUM_CHILD_PIPES; j++) {
                    if (j != i) {
                        close(pipes_out[j][0]);
                        close(pipes_out[j][1]);
                    }
                }
                close(pipes_out[i][0]);
                close(pipes_in[0]);
                close(pipes_in[1]);

                char write_pipe_str[10];
                snprintf(write_pipe_str, sizeof(write_pipe_str), "%d", pipes_out[i][1]);
                execl(child_executables[i], child_executables[i], write_pipe_str, logfile_fd_str, NULL);
            }
            if (i == 1) {
                // * Close all not needed pipes
                for (int j = 0; j < NUM_CHILD_PIPES; j++) {
                    if (j != i) {
                        close(pipes_out[j][0]);
                        close(pipes_out[j][1]);
                    }
                }
                close(pipes_in[0]);
                close(pipes_in[1]);

                // * The obstacle process only send data to the target process
                close(pipes_out[1][0]);

                char write_pipe_str[10];
                snprintf(write_pipe_str, sizeof(write_pipe_str), "%d", pipes_out[i][1]);
                execl(child_executables[i], child_executables[i], write_pipe_str,
                    logfile_fd_str, NULL);
            }
            if (i == 2) {
                // * Close all not needed pipes
                for (int j = 0; j < NUM_CHILD_PIPES; j++) {
                    if (j != i-1) {
                        close(pipes_out[j][0]);
                        close(pipes_out[j][1]);
                    }
                }
                close(pipes_in[0]);
                close(pipes_in[1]);
                // * The obstacle process only send data to the target process
                close(pipes_out[1][1]);

                char read_pipe_str[10];
                snprintf(read_pipe_str, sizeof(read_pipe_str), "%d", pipes_out[i][0]);
                execl(child_executables[i], child_executables[i], read_pipe_str,
                    logfile_fd_str, NULL);
            }
            else {
                // * Close all not needed pipes
                for (int j = 0; j < NUM_CHILD_PIPES; j++) {
                    if (j != i-1) {
                        close(pipes_out[j][0]);
                        close(pipes_out[j][1]);
                    }
                }
                close(pipes_in[1]);
                close(pipes_out[i][0]);

                char read_pipe_str[10], write_pipe_str[10];
                snprintf(read_pipe_str, sizeof(read_pipe_str), "%d", pipes_in[0]);
                snprintf(write_pipe_str, sizeof(write_pipe_str), "%d", pipes_out[i][1]);
                execl(child_executables[i], child_executables[i], read_pipe_str, write_pipe_str,
                    logfile_fd_str, NULL);
            }

            // * If execl returns, an error occurred
            perror("execl");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

pid_t create_watchdog_process(const pid_t pgid, const int logfile_fd) {
    /*
     * Function to create the watchdog process.
     * @return PID of the watchdog in case of success, -1 in case of failure.
     */
    const pid_t watchdog_pid = fork();
    if (watchdog_pid < 0) {
        perror("fork watchdog");
        return -1;
    }
    if (watchdog_pid == 0) {
        char pgid_str[10];
        snprintf(pgid_str, sizeof(pgid_str), "%d", pgid);
        char logfile_fd_str[10];
        snprintf(logfile_fd_str, sizeof(logfile_fd_str), "%d", logfile_fd);
        // * Execute the watchdog executable
        execl("./watchdog", "watchdog", pgid_str, logfile_fd_str, NULL);


        // * If execl returns, an error occurred
        perror("execl");
        exit(EXIT_FAILURE);
    }

    return watchdog_pid;
}

pid_t create_blackboard_process(int pipes_in[NUM_CHILD_PIPES-2][2], int pipes_out[2],
    const pid_t watchdog_pid, int logfile_fd, pid_t pgid) {
    /*
     * Function to create the blackboard process.
     * @param pipes Array containing the file descriptors of the pipes.
     * @param watchdog_pid PID of the watchdog process.
     * @return PID of the blackboard in case of success, -1 in case of failure.
    */
    const pid_t blackboard_pid = fork();
    if (blackboard_pid < 0) {
        perror("fork blackboard");
        return -1;
    }
    if (blackboard_pid == 0) {
        // * Join the main process's process group
        if (setpgid(0, pgid) == -1) {
            perror("setpgid in child");
            exit(EXIT_FAILURE);
        }

        // * Close all not needed pipes
        for (int i = 0; i < NUM_CHILD_PIPES; i++) {
            close(pipes_in[i][1]);
        }
        close(pipes_in[1][0]);
        close(pipes_out[0]);

        /*
         * Prepare arguments for the blackboard executable
         * Pass all read pipe descriptors and the watchdog PID
         * args[0] = "./blackboard"
         * args[1..NUM_CHILD_PIPES] = read_fds
         * args[NUM_CHILD_PIPES + 1..2*NUM_CHILD_PIPES - 1] = write_fds (excluding keyboard_manager)
         * args[2*NUM_CHILD_PIPES] = watchdog_pid
         * args[2*NUM_CHILD_PIPES + 1] = logfile file descriptor
        */
        char watchdog_pid_str[10];
        snprintf(watchdog_pid_str, sizeof(watchdog_pid_str), "%d", watchdog_pid);
        char logfile_fd_str[10];
        snprintf(logfile_fd_str, sizeof(logfile_fd_str), "%d", logfile_fd);

        // * Allocate memory for arguments
        const int total_args = 2 * NUM_CHILD_PIPES;
        char **args = malloc(total_args * sizeof(char *));
        if (!args) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        args[0] = "./blackboard";

        // * Add all read_fds
        int arg_index = 1;

        char *read_pipe_str = malloc(10);
        snprintf(read_pipe_str, 10, "%d", pipes_in[0][0]);
        args[arg_index++] = read_pipe_str;
        read_pipe_str = malloc(10);
        snprintf(read_pipe_str, 10, "%d", pipes_in[2][0]);
        args[arg_index++] = read_pipe_str;

        char *write_pipe_str = malloc(10);
        snprintf(write_pipe_str, 10, "%d", pipes_out[1]);
        args[arg_index++] = write_pipe_str;

        // * Add watchdog_pid
        args[arg_index++] = watchdog_pid_str;
        args[arg_index++] = logfile_fd_str;
        args[arg_index] = NULL; // * NULL terminate the argument list
        // * Execute the blackboard executable with the necessary arguments
        execvp(args[0], args);

        // * If execvp returns, an error occurred
        perror("execvp blackboard");

        // * Free allocated memory
        for (int i = 1; i < arg_index; i++) {
            free(args[i]);
        }
        free(args);
        exit(EXIT_FAILURE);
    }

    return blackboard_pid;
}
