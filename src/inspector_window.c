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
    // Inizializza ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(FALSE);
    start_color();

    // Imposta la coppia di colori: testo bianco su sfondo rosso
    init_pair(4, COLOR_WHITE, COLOR_RED);

    // Ottieni le dimensioni dello schermo
    int insp_height, insp_width;
    getmaxyx(stdscr, insp_height, insp_width);

    // Crea la finestra principale di inspection (occupante l'intero schermo)
    WINDOW *inspect_win = newwin(insp_height, insp_width, 0, 0);
    box(inspect_win, 0, 0);
    wrefresh(inspect_win);
    refresh();

    // Calcola la metà della larghezza per suddividere in due riquadri
    int half_width = insp_width / 2;

    // Crea due subfinestre:
    // - left_box per i dati del drone.
    // - right_box per il tastierino.
    WINDOW *left_box = newwin(insp_height - 2, half_width - 2, 1, 1);
    WINDOW *right_box = newwin(insp_height - 2, insp_width - half_width - 2, 1, half_width + 1);

    // Disegna i bordi per ciascun box
    box(left_box, 0, 0);
    box(right_box, 0, 0);

    // Contenuto iniziale per left_box
    mvwprintw(left_box, 1, 1, "Drone Position: N/A");
    mvwprintw(left_box, 2, 1, "Velocity: N/A");
    mvwprintw(left_box, 3, 1, "Force: N/A");
    wrefresh(left_box);

    // -----------------------------------------------
    // Disegna il tastierino iniziale al centro di right_box
    // I tasti sono:
    //  - RIGA 1: [w]  [e]  [r]
    //  - RIGA 2: [s]  [d]  [f]
    //  - RIGA 3: [x]  [c]  [v]
    // -----------------------------------------------
    int rH, rW;
    getmaxyx(right_box, rH, rW);
    int inner_width = rW - 2;   // Escludiamo i bordi
    int inner_height = rH - 2;
    int total_length = 13;      // "[?]" (3 caratteri) + 2 spazi fra i bottoni per 3 bottoni
    int start_col = 1 + (inner_width - total_length) / 2;
    int start_row = 1 + (inner_height - 3) / 2;

    // RIGA 1: [w] [e] [r]
    mvwprintw(right_box, start_row, start_col, "[w]");
    mvwprintw(right_box, start_row, start_col + 5, "[e]");
    mvwprintw(right_box, start_row, start_col + 10, "[r]");
    // RIGA 2: [s] [d] [f]
    mvwprintw(right_box, start_row + 1, start_col, "[s]");
    mvwprintw(right_box, start_row + 1, start_col + 5, "[d]");
    mvwprintw(right_box, start_row + 1, start_col + 10, "[f]");
    // RIGA 3: [x] [c] [v]
    mvwprintw(right_box, start_row + 2, start_col, "[x]");
    mvwprintw(right_box, start_row + 2, start_col + 5, "[c]");
    mvwprintw(right_box, start_row + 2, start_col + 10, "[v]");
    wrefresh(right_box);
    // -----------------------------------------------

    // Ciclo principale: legge dal FIFO e aggiorna i box
    while (keep_running) {
        char insp_msg[128] = {0};
        int fd = open(INSPECTOR_FIFO, O_RDONLY);
        if (fd == -1) {
            perror("open");
            usleep(100000);
            continue;
        }
        int ret = read(fd, insp_msg, sizeof(insp_msg) - 1);  // -1 per garantire la terminazione
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
        insp_msg[ret] = '\0';  // Assicurati che la stringa sia terminata
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
            // Aggiorna il left_box con i nuovi dati del drone
            wclear(left_box);
            box(left_box, 0, 0);
            mvwprintw(left_box, 1, 1, "Drone Position: (%d, %d)", pos_x, pos_y);
            mvwprintw(left_box, 2, 1, "Velocity: (%d, %d)", vel_x, vel_y);
            mvwprintw(left_box, 3, 1, "Force: (%d, %d)", force_x, force_y);
            wrefresh(left_box);

            // Aggiorna il tastierino in right_box
            wclear(right_box);
            box(right_box, 0, 0);
            // (Ricalcoliamo le posizioni per centrare il tastierino)
            getmaxyx(right_box, rH, rW);
            inner_width = rW - 2;
            inner_height = rH - 2;
            start_col = 1 + (inner_width - total_length) / 2;
            start_row = 1 + (inner_height - 3) / 2;

            // RIGA 1: tasti [w] [e] [r]
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

            // RIGA 2: tasti [s] [d] [f]
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

            // RIGA 3: tasti [x] [c] [v]
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

    // Cleanup
    delwin(left_box);
    delwin(right_box);
    delwin(inspect_win);
    endwin();
    return EXIT_SUCCESS;
}

void signal_close(int signum) {
    keep_running = 0;
}