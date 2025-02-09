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
#include <errno.h>
#include <string.h>

int get_random_interval(int min, int max);
long long get_log_timestamp_from_FILE(FILE *file);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        perror("Usage: Watchdog <process_group_id> <logfile_fd>");
        exit(EXIT_FAILURE);
    }

    // * Parse the PGID
    pid_t pgid = (pid_t) atoi(argv[1]);
    if (pgid <= 0) {
        perror("Invalid PGID");
        exit(EXIT_FAILURE);
    }
    // Parse logfile file descriptor and open it
    int logfile_fd = atoi(argv[2]);
    FILE *logfile = fdopen(logfile_fd, "r");
    if (!logfile) {
        perror("fdopen logfile");
        return EXIT_FAILURE;
    }
    srand(time(NULL) ^ getpid());
    while (1) {
        time_t mod_before = get_log_timestamp_from_FILE(logfile);

        // Invia SIGUSR1 al gruppo di processi (PGID negativo)
        if (kill(-pgid, SIGUSR1) == -1) {
            perror("kill(SIGUSR1)");
            sleep(10);
            exit(EXIT_FAILURE);
        }

        // Attendi un intervallo per dare tempo ai processi di reagire
        sleep(get_random_interval(4, 10));

        // Rileva il nuovo timestamp
        long long mod_after = get_log_timestamp_from_FILE(logfile);
        if (mod_after <= mod_before) {
            fprintf(stderr, "No response, quit. Group: %d\n", pgid);
            kill(-pgid, SIGTERM);
            sleep(2);
            kill(-pgid, SIGKILL);
            break;
        }
    }
    return 0;
}

int get_random_interval(int min, int max) {
    return min + rand() % (max - min + 1);
}

long long get_log_timestamp_from_FILE(FILE *file) {
    struct stat file_stat;
    int fd = fileno(file);
    if (fstat(fd, &file_stat) == 0) {
#ifdef __linux__
        // Usa st_mtim per avere risoluzione in nanosecondi
        return (long long) file_stat.st_mtim.tv_sec * 1000000000LL + file_stat.st_mtim.tv_nsec;
#else
        return file_stat.st_mtime; // Fallback su altri sistemi
#endif
    }
    return 0;
}