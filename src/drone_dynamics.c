//
// Created by Gian Marco Balia
//
// src/drone_dynamics.c
#include <macros.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <ncurses.h>
#include "macros.h"

FILE *logfile;

void signal_triggered(int signum) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  fprintf(logfile, "[%02d:%02d:%02d] PID: %d - %s\n", t->tm_hour, t->tm_min, t->tm_sec, getpid(),
      "Dynamics is active.");
  fflush(logfile);
}

int main(int argc, char *argv[]) {
  /*
   * Dynamics process
   * @param argv[1]: Read file descriptors
   * @param argv[2]: Write file descriptors
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

  if (argc != 4) {
    fprintf(stderr, "Usage: %s <read_fd> <write_fd> <logfile_fd>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int read_fd = atoi(argv[1]);
  if (read_fd <= 0) {
    fprintf(stderr, "Invalid read file descriptor: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  int write_fd = atoi(argv[2]);
  if (write_fd <= 0) {
    fprintf(stderr, "Invalid write file descriptor: %s\n", argv[2]);
    return EXIT_FAILURE;
  }

  // * Parse logfile file descriptors
  int logfile_fd = atoi(argv[argc - 1]);
  logfile = fdopen(logfile_fd, "a");
  if (!logfile) {
    perror("fdopen logfile");
    return EXIT_FAILURE;
  }

  while(1) {
    // * Receive the updated map
    char grid[GAME_HEIGHT][GAME_WIDTH];
    memset(grid, ' ', GAME_HEIGHT*GAME_WIDTH);
    if (read(read_fd, grid, GAME_HEIGHT * GAME_WIDTH * sizeof(char)) != GAME_HEIGHT * GAME_WIDTH * sizeof(char)) {
      perror("read grid");
      return EXIT_FAILURE;
    }
    // * Read the drone position and force
    char msg[100];
    if (read(read_fd, msg, sizeof(msg)) == -1) {
      perror("read");
      return EXIT_FAILURE;
    }
    int x[2], y[2], force_x, force_y;
    if (sscanf(msg, "%d,%d,%d,%d,%d,%d", &x[0], &y[0], &x[1], &y[1], &force_x, &force_y) != 6) {
      fprintf(stderr, "Failed to parse message: %s\n", msg);
      return EXIT_FAILURE;
    }
    // * Total force
    double Fx = (double)force_x/10, Fy = (double)force_y/10;

    for (int i = 0; i < GAME_HEIGHT; i++) {
      for (int j = 0; j < GAME_WIDTH; j++) {
        const char cell = grid[i][j];
        if (cell == ' ') continue;
        const int dx = x[1] - j;
        const int dy = y[1] - i;
        double dist = sqrt((double)dx*dx + (double)dy*dy);

        dist = dist < MIN_RHO_OBST ? MIN_RHO_OBST : dist;
        if (dist < RHO_OBST && cell == 'o') {
          Fx += ETA*(1/dist - 1/RHO_OBST)*dx/pow(dist,3);
          Fy += ETA*(1/dist - 1/RHO_OBST)*dy/pow(dist,3);
          continue;
        }

        dist = sqrt((double)dx*dx + (double)dy*dy);
        dist = dist < MIN_RHO_TRG ? MIN_RHO_TRG : dist;
        if (dist < RHO_TRG && strchr("0123456789", cell)) {
          Fx -= EPSILON*(double)dx/dist;
          Fy -= EPSILON*(double)dy/dist;
        }
      }
    }

    // * Compute the position from the force
    int x_new = (int)(
            (TIME*TIME*Fx - DRONE_MASS*x[0] + (2*DRONE_MASS + DAMPING*TIME)*x[1]) / (DRONE_MASS + DAMPING*TIME)
        );
    int y_new = (int)(
        (TIME*TIME*Fy - DRONE_MASS*y[0] + (2*DRONE_MASS + DAMPING*TIME)*y[1]) / (DRONE_MASS + DAMPING*TIME)
    );

    // TODO: delete this two rows
    x_new = x[1] + force_x;
    y_new = y[1] + force_y;

    // * Clamp to window boundaries so we do not jump outside:
    // ! Notice that the min position is 2 and the max is MAX-3. This to guarantee that the drone stay inside the map
    // ! also when the window is rescaled (height/GAME_HEIGHT is a double and the convertion can take the drone on
    // ! the border)
    if (x_new < 2) {
      x_new = 2;
    } else if (x_new > GAME_WIDTH - 3) {
      x_new = GAME_WIDTH - 3;
    }

    if (y_new < 2) {
      y_new = 2;
    } else if (y_new > GAME_HEIGHT - 3) {
      y_new = GAME_HEIGHT - 3;
    }

    char out_buf[32];
    sprintf(out_buf, "%d,%d", x_new, y_new);
    if (write(write_fd, out_buf, sizeof(out_buf)) == -1) {
      perror("write");
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
