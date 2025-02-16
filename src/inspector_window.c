//
// Created by Gian Marco Balia
//
// src/inspector_window.c

#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <errno.h>

#include "macros.h"

static volatile sig_atomic_t keep_running = 1;

void signal_close(int signum);

int main() {
    // * Initialize ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(FALSE);
    start_color();
    // *White text with red background
    init_pair(4, COLOR_WHITE, COLOR_RED);
    // * Make the window
    int insp_height, insp_width;
    getmaxyx(stdscr, insp_height, insp_width);
    WINDOW *inspect_win = newwin(insp_height, insp_width, 0, 0);
    box(inspect_win, 0, 0);
    wrefresh(inspect_win);
    refresh();
    // *Make two subwindows:
    // *    - left_box: drone's data.
    // *    - right_box: keypad.
    int half_width = insp_width / 2;
    WINDOW *left_box = newwin(insp_height - 2, half_width - 2, 1, 1);
    WINDOW *right_box = newwin(insp_height - 2, insp_width - half_width - 2, 1, half_width + 1);
    // * Draw bordes
    box(left_box, 0, 0);
    box(right_box, 0, 0);
    // * Initialise left_box
    mvwprintw(left_box, 1, 1, "Drone Position: N/A");
    mvwprintw(left_box, 2, 1, "Velocity: N/A");
    mvwprintw(left_box, 3, 1, "Force: N/A");
    wrefresh(left_box);
    // * Draw the keypad
    // * - 1st rop: [w]  [e]  [r]
    // * - 2nd rop: [s]  [d]  [f]
    // * - 3rd row: [x]  [c]  [v]
    int rH, rW;
    getmaxyx(right_box, rH, rW);
    int inner_width = rW - 2;
    int inner_height = rH - 2;
    int total_length = 13;
    int start_col = 1 + (inner_width - total_length) / 2;
    int start_row = 1 + (inner_height - 3) / 2;
    // * 1st rop: [w]  [e]  [r]
    mvwprintw(right_box, start_row, start_col, "[w]");
    mvwprintw(right_box, start_row, start_col + 5, "[e]");
    mvwprintw(right_box, start_row, start_col + 10, "[r]");
    // * 2nd rop: [s]  [d]  [f]
    mvwprintw(right_box, start_row + 1, start_col, "[s]");
    mvwprintw(right_box, start_row + 1, start_col + 5, "[d]");
    mvwprintw(right_box, start_row + 1, start_col + 10, "[f]");
    // * 3rd row: [x]  [c]  [v]
    mvwprintw(right_box, start_row + 2, start_col, "[x]");
    mvwprintw(right_box, start_row + 2, start_col + 5, "[c]");
    mvwprintw(right_box, start_row + 2, start_col + 10, "[v]");
    wrefresh(right_box);

    while (keep_running) {
        char insp_msg[128] = {0};
        int fd = open(INSPECTOR_FIFO, O_RDONLY);
        if (fd == -1) {
            continue;
        }
        int ret = read(fd, insp_msg, sizeof(insp_msg) - 1);
        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                close(fd);
                continue;
            } else {
                perror("read");
                close(fd);
                return EXIT_FAILURE;
            }
        }
        insp_msg[ret] = '\0';
        close(fd);

        if (ret > 0) {
            char c;
            int force_x, force_y, pos_x, pos_y, vel_x, vel_y;
            if (sscanf(insp_msg, "%d,%d,%d,%d,%d,%d,%c",
                       &force_x, &force_y, &pos_x, &pos_y, &vel_x, &vel_y, &c) != 7) {
                mvwprintw(left_box, 5, 1, "Invalid message format:");
                mvwprintw(left_box, 6, 1, "%s", insp_msg);
                wrefresh(left_box);
                continue;
            }
            // * Update the kaypad in the left_box
            wclear(left_box);
            box(left_box, 0, 0);
            mvwprintw(left_box, 1, 1, "Drone Position: (%d, %d)", pos_x, pos_y);
            mvwprintw(left_box, 2, 1, "Velocity: (%d, %d)", vel_x, vel_y);
            mvwprintw(left_box, 3, 1, "Force: (%d, %d)", force_x, force_y);
            wrefresh(left_box);
            // * Update the keypad in the right_box
            wclear(right_box);
            box(right_box, 0, 0);
            getmaxyx(right_box, rH, rW);
            inner_width = rW - 2;
            inner_height = rH - 2;
            start_col = 1 + (inner_width - total_length) / 2;
            start_row = 1 + (inner_height - 3) / 2;
            // * [w] [e] [r]
            if (c == 'w') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row, start_col, "[w]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row, start_col, "[w]");
            }
            if (c == 'e') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row, start_col + 5, "[e]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row, start_col + 5, "[e]");
            }
            if (c == 'r') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row, start_col + 10, "[r]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row, start_col + 10, "[r]");
            }
            // * [s] [d] [f]
            if (c == 's') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row + 1, start_col, "[s]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row + 1, start_col, "[s]");
            }
            if (c == 'd') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row + 1, start_col + 5, "[d]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row + 1, start_col + 5, "[d]");
            }
            if (c == 'f') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row + 1, start_col + 10, "[f]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row + 1, start_col + 10, "[f]");
            }
            // * [x] [c] [v]
            if (c == 'x') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row + 2, start_col, "[x]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row + 2, start_col, "[x]");
            }
            if (c == 'c') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row + 2, start_col + 5, "[c]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row + 2, start_col + 5, "[c]");
            }
            if (c == 'v') {
                wattron(right_box, COLOR_PAIR(4));
                mvwprintw(right_box, start_row + 2, start_col + 10, "[v]");
                wattroff(right_box, COLOR_PAIR(4));
            } else {
                mvwprintw(right_box, start_row + 2, start_col + 10, "[v]");
            }
        }
        wrefresh(right_box);
    }
    // * Cleanup
    delwin(left_box);
    delwin(right_box);
    delwin(inspect_win);
    endwin();
    return EXIT_SUCCESS;
}

void signal_close(int signum) {
    keep_running = 0;
}