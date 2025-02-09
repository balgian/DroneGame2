//
// Created by Gian Marco Balia
//
// src/blackboard.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <ncurses.h>
#include <fcntl.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <atomic>
#include <vector>
#include <random>
#include <algorithm>

#include "macros.h"
#include "ObstaclesPubSubTypes.hpp"
#include "TargetsPubSubTypes.hpp"

/**
 * @file CustomTransportSubscriber.cpp
 *
 * Subscriber che si iscrive a due topic distinti:
 *   - "topic 1" per i messaggi di tipo Obstacles:
 *       struct Obstacles {
 *           sequence<long> obstacles_x;
 *           sequence<long> obstacles_y;
 *           long obstacles_number;
 *       };
 *
 *   - "topic 2" per i messaggi di tipo Targets:
 *       struct Targets {
 *           sequence<long> targets_x;
 *           sequence<long> targets_y;
 *           long targets_number;
 *       };
 */

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.hpp>

using namespace eprosima::fastdds::dds;
using namespace std::chrono_literals;

// Subscriber che gestisce i messaggi Obstacles
class ObstaclesListener : public DataReaderListener {
public:
    std::atomic_int samples_;
    Obstacles obstacles_msg_;
    ObstaclesListener() : samples_(0) {}
    ~ObstaclesListener() override {}

    void on_subscription_matched(DataReader* reader, const SubscriptionMatchedStatus &info) override
    {
        if (info.current_count_change == 1)
        {
            std::cout << "Obstacles Subscriber matched." << std::endl;
        }
        else if (info.current_count_change == -1)
        {
            std::cout << "Obstacles Subscriber unmatched." << std::endl;
        }
    }

    void on_data_available(DataReader* reader) override {
        SampleInfo info;
        if (reader->take_next_sample(&obstacles_msg_, &info) == RETCODE_OK)
        {
            if (info.valid_data)
            {
                samples_++;
                std::cout << "Obstacles Sample #" << samples_ << ": "
                          << "Number of obstacles: " << obstacles_msg_.obstacles_number() << std::endl;
                const auto & xs = obstacles_msg_.obstacles_x();
                const auto & ys = obstacles_msg_.obstacles_y();
                for (size_t i = 0; i < xs.size(); i++)
                {
                    std::cout << "  (" << xs[i] << ", " << ys[i] << ")";
                }
                std::cout << std::endl;
            }
        }
    }
};

// Subscriber che gestisce i messaggi Targets
class TargetsListener : public DataReaderListener
{
public:
    std::atomic_int samples_;
    Targets targets_msg_;

    TargetsListener() : samples_(0) { }
    ~TargetsListener() override { }

    void on_subscription_matched(
        DataReader* reader,
        const SubscriptionMatchedStatus &info) override
    {
        if (info.current_count_change == 1)
        {
            std::cout << "Targets Subscriber matched." << std::endl;
        }
        else if (info.current_count_change == -1)
        {
            std::cout << "Targets Subscriber unmatched." << std::endl;
        }
    }

    void on_data_available(DataReader* reader) override
    {
        SampleInfo info;
        if (reader->take_next_sample(&targets_msg_, &info) == RETCODE_OK)
        {
            if (info.valid_data)
            {
                samples_++;
                std::cout << "Targets Sample #" << samples_ << ": "
                          << "Number of targets: " << targets_msg_.targets_number() << std::endl;
                const auto & xs = targets_msg_.targets_x();
                const auto & ys = targets_msg_.targets_y();
                for (size_t i = 0; i < xs.size(); i++)
                {
                    std::cout << "  (" << xs[i] << ", " << ys[i] << ")";
                }
                std::cout << std::endl;
            }
        }
    }
};

class CustomTransportSubscriber {
private:
    DomainParticipant *participant_;
    Subscriber *subscriber_;

    // Topics e DataReaders per Obstacles e Targets
    Topic *obstacles_topic_;
    DataReader *obstacles_reader_;
    Topic *targets_topic_;
    DataReader *targets_reader_;

    // TypeSupport per i due tipi
    TypeSupport obstacles_type_;
    TypeSupport targets_type_;

    ObstaclesListener obstacles_listener_;
    TargetsListener targets_listener_;

public:
    CustomTransportSubscriber()
        : participant_(nullptr)
        , subscriber_(nullptr)
        , obstacles_topic_(nullptr)
        , obstacles_reader_(nullptr)
        , targets_topic_(nullptr)
        , targets_reader_(nullptr)
        , obstacles_type_(new ObstaclesPubSubType())
        , targets_type_(new TargetsPubSubType())
    { }

    virtual ~CustomTransportSubscriber()
    {
        if (obstacles_reader_ != nullptr)
        {
            subscriber_->delete_datareader(obstacles_reader_);
        }
        if (targets_reader_ != nullptr)
        {
            subscriber_->delete_datareader(targets_reader_);
        }
        if (obstacles_topic_ != nullptr)
        {
            participant_->delete_topic(obstacles_topic_);
        }
        if (targets_topic_ != nullptr)
        {
            participant_->delete_topic(targets_topic_);
        }
        if (subscriber_ != nullptr)
        {
            participant_->delete_subscriber(subscriber_);
        }
        DomainParticipantFactory::get_instance()->delete_participant(participant_);
    }

    //! Inizializza il subscriber per entrambi i topic.
    bool init() {
        DomainParticipantQos participantQos;
        participantQos.name("Participant_subscriber");
        participant_ = DomainParticipantFactory::get_instance()->create_participant(1, participantQos);
        if (participant_ == nullptr)
        {
            return false;
        }

        // Registra i due tipi
        obstacles_type_.register_type(participant_);
        targets_type_.register_type(participant_);

        // Crea i topic con nomi "topic 1" e "topic 2"
        obstacles_topic_ = participant_->create_topic("topic 1", obstacles_type_.get_type_name(), TOPIC_QOS_DEFAULT);
        if (obstacles_topic_ == nullptr)
        {
            return false;
        }
        targets_topic_ = participant_->create_topic("topic 2", targets_type_.get_type_name(), TOPIC_QOS_DEFAULT);
        if (targets_topic_ == nullptr)
        {
            return false;
        }

        // Crea il Subscriber
        subscriber_ = participant_->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr);
        if (subscriber_ == nullptr)
        {
            return false;
        }

        // Crea il DataReader per Obstacles
        obstacles_reader_ = subscriber_->create_datareader(obstacles_topic_, DATAREADER_QOS_DEFAULT, &obstacles_listener_);
        if (obstacles_reader_ == nullptr)
        {
            return false;
        }

        // Crea il DataReader per Targets
        targets_reader_ = subscriber_->create_datareader(targets_topic_, DATAREADER_QOS_DEFAULT, &targets_listener_);
        if (targets_reader_ == nullptr)
        {
            return false;
        }

        return true;
    }

    void run(char grid[GAME_HEIGHT][GAME_WIDTH]) {
    // Attende finché non viene ricevuto almeno un messaggio per ciascun topic.
    while (obstacles_listener_.samples_ == 0 || targets_listener_.samples_ == 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Inizializza la griglia: imposta tutti i caratteri a spazio.
    memset(grid, ' ', sizeof(char) * GAME_HEIGHT * GAME_WIDTH);

    // --- Popola la griglia con i dati degli ostacoli (riproporzionando le coordinate) ---
    size_t obs_count = obstacles_listener_.obstacles_msg_.obstacles_x().size();
    for (size_t i = 0; i < obs_count; i++)
    {
        int orig_x = obstacles_listener_.obstacles_msg_.obstacles_x()[i];
        int orig_y = obstacles_listener_.obstacles_msg_.obstacles_y()[i];
        // Riproporziona le coordinate: si assume che i valori originali siano nell'intervallo [0, obs_count]
        int new_x = (GAME_WIDTH * orig_x) / obs_count;
        int new_y = (GAME_HEIGHT * orig_y) / obs_count;
        // Correggi eventuali out-of-bound
        if (new_x < 0) new_x = 0;
        if (new_x >= GAME_WIDTH) new_x = GAME_WIDTH - 1;
        if (new_y < 0) new_y = 0;
        if (new_y >= GAME_HEIGHT) new_y = GAME_HEIGHT - 1;
        grid[new_y][new_x] = 'o';  // 'o' per indicare un ostacolo
    }

    // --- Popola la griglia con i dati dei target, assegnando ad ognuno un numero casuale univoco ---
    // Prepara un vettore di cifre (caratteri) da '0' a '9'
    std::vector<char> digits = {'0','1','2','3','4','5','6','7','8','9'};
    // Mescola il vettore per ottenere un ordine casuale
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(digits.begin(), digits.end(), g);

    size_t trg_count = targets_listener_.targets_msg_.targets_x().size();
    // Per ogni target, riproporziona le coordinate e assegna una cifra unica (se disponibile)
    for (size_t i = 0; i < trg_count && i < digits.size(); i++)
    {
        int orig_x = targets_listener_.targets_msg_.targets_x()[i];
        int orig_y = targets_listener_.targets_msg_.targets_y()[i];
        // Riproporziona: si assume che i valori originali siano nell'intervallo [0, trg_count]
        int new_x = (GAME_WIDTH * orig_x) / trg_count;
        int new_y = (GAME_HEIGHT * orig_y) / trg_count;
        if (new_x < 0) new_x = 0;
        if (new_x >= GAME_WIDTH) new_x = GAME_WIDTH - 1;
        if (new_y < 0) new_y = 0;
        if (new_y >= GAME_HEIGHT) new_y = GAME_HEIGHT - 1;
        // Assegna la cifra (non ripetuta) al target nella griglia
        grid[new_y][new_x] = digits[i];
    }
}
};

FILE *logfile;
// Questa variabile viene modificata dal signal handler
volatile sig_atomic_t sig_received = 0;

/**
 * Parse the file descriptors and watchdog PID from the command-line arguments.
 * @param argc Number of arguments.
 * @param argv Array of arguments.
 * @param read_fds Array to store read file descriptors.
 * @param write_fds Array to store write file descriptor.
 * @param watchdog_pid Pointer to store the watchdog PID.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int parser(int argc, char *argv[], int *read_fds, int *write_fds, pid_t *watchdog_pid) {
    // * Parse read file descriptors
    for (int i = 0; i < NUM_CHILD_PIPES-1; i++) {
        char *endptr;
        read_fds[i] = strtol(argv[i + 1], &endptr, 10);
        if (*endptr != '\0' || read_fds[i] < 0) {
            fprintf(stderr, "Invalid read file descriptor BLACK: %s\n", argv[i + 1]);
            return EXIT_FAILURE;
        }
    }

    // * Parse write file descriptors (excluding keyboard_manager)
    char *endptr;
    *write_fds = strtol(argv[NUM_CHILD_PIPES], &endptr, 10);
    if (*endptr != '\0' || *write_fds < 0) {
        fprintf(stderr, "Invalid write file descriptor: %s\n", argv[NUM_CHILD_PIPES]);
        return EXIT_FAILURE;
    }

    // * Parse watchdog PID
    char *endptrwd;
    *watchdog_pid = strtol(argv[argc - 2], &endptrwd, 10);
    if (*endptrwd != '\0' || *watchdog_pid <= 0) {
        fprintf(stderr, "Invalid watchdog PID: %s\n", argv[argc - 2]);
        return EXIT_FAILURE;
    }

    // Parse logfile file descriptor and open it
    int logfile_fd = atoi(argv[argc - 1]);
    logfile = fdopen(logfile_fd, "a");
    if (!logfile) {
        perror("fdopen logfile");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * Signal handler that writes a message to the logfile when a signal is received.
 *
 * @param signum The signal number.
 */
void signal_triggered(int signum) {
    sig_received = 1;
}

/**
 * Initialize ncurses settings and create a new window.
 *
 * @return Pointer to the newly created window, or NULL on failure.
 */
int initialize_ncurses() {
    /*
     * Initialize ncurses settings.
     */

    // * Start curses mode
    if (initscr() == NULL) {
        return EXIT_FAILURE;
    }
    cbreak();               // * Disable line buffering
    noecho();               // * Don't echo pressed keys
    curs_set(FALSE);        // * Hide the cursor
    start_color();          // * Enable color functionality
    use_default_colors();   // * Allow default terminal colors

    // * Initialize color pairs
    init_pair(1, COLOR_BLUE, -1); // * Drone
    init_pair(2, COLOR_GREEN, -1); // * Targets
    init_pair(3, COLOR_YELLOW, -1); // * Obstacles

    return EXIT_SUCCESS;
}

/**
 * Modify the drone force based on the input key.
 *
 * Command keys:
 * 'w': Up Left, 'e': Up, 'r': Up Right or Reset,
 * 's': Left or Suspend, 'd': Brake, 'f': Right,
 * 'x': Down Left, 'c': Down, 'v': Down Right,
 * 'p': Pause, 'q': Quit
 *
 * @param drone_force Array representing the drone's force.
 * @param c The input character.
 */
void command_drone(int *drone_force, char c) {
    if (strchr("wsx", c)) {
        drone_force[0]--;
    }
    if (strchr("rfv", c)) {
        drone_force[0]++;
    }
    if (strchr("wer", c)) {
        drone_force[1]--;
    }
    if (strchr("xcv", c)) {
        drone_force[1]++;
    }
    if (c == 'd') {
        drone_force[0] = 0;
        drone_force[1] = 0;
    }
}

pid_t launch_inspection_window() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        if (setpgid(0, 0) == -1) {
            perror("setpgid");
            exit(EXIT_FAILURE);
        }
        // Lancia gnome-terminal con l'opzione --disable-factory e passa il parametro al programma inspector
        execlp("gnome-terminal", "gnome-terminal", "--disable-factory", "--", "bash", "-c", "./inspector; exec bash", (char *)NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    }
    return pid;
}

// This function samples many points along the straight-line path from (x0,y0) to (x1,y1).
// If any of the sampled cells contains a target (a digit in "0123456789"), it removes it.
void remove_target_on_path_oversample(char grid[GAME_HEIGHT][GAME_WIDTH],
                                        int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    // If the movement is mostly horizontal, use a higher oversampling factor.
    int oversample;
    if (dy == 0 && dx > 0) {
        oversample = 10;  // Increase oversampling for horizontal-only moves
    } else {
        oversample = 5;   // Otherwise, use the default factor
    }

    int steps = (dx > dy ? dx : dy) * oversample;
    if (steps == 0) {
        if (x0 >= 0 && x0 < GAME_WIDTH && y0 >= 0 && y0 < GAME_HEIGHT) {
            if (strchr("0123456789", grid[y0][x0]) != NULL)
                grid[y0][x0] = ' ';
        }
        return;
    }

    for (int i = 0; i <= steps; i++) {
        double t = (double)i / steps;
        int x = x0 + (int)round((x1 - x0) * t);
        int y = y0 + (int)round((y1 - y0) * t);
        if (x >= 0 && x < GAME_WIDTH && y >= 0 && y < GAME_HEIGHT) {
            if (strchr("0123456789", grid[y][x]) != NULL) {
                grid[y][x] = ' ';
            }
        }
    }
}

/**
 * Main function for the blackboard process.
 *
 * @param argc Argument count.
 * @param argv Array of command-line arguments.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 *
 * The command-line arguments are:
 * - argv[1..N]: Read file descriptors
 * - argv[N+1..2N-1]: Write file descriptors (excluding keyboard_manager)
 * - argv[2N]: Watchdog PID
 * - argv[2N+1]: Logfile file descriptor
 */
int main(const int argc, char *argv[]) {
    // * Define the signal action
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_triggered;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    if (argc != 2 * NUM_CHILD_PIPES + 1) {
        fprintf(stderr, "Usage: %s <read_fd_keyboard> <read_fd_dynamics> <write_fd_dynamics> <watchdog_pid>"
                        "<logfile_fd>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // * Parse argv
    int read_fds[NUM_CHILD_PIPES-1];
    int write_fds;
    pid_t watchdog_pid = 0;
    if (parser(argc, argv, read_fds, &write_fds, &watchdog_pid) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    // ! Map the child pipes to more meaningful names
    const int keyboard = read_fds[0];
    const int dynamic_read = read_fds[1];

    const int dynamic_write = write_fds;

    // *
    mkfifo(INSPECTOR_FIFO, 0666);

    // * Initialise window's game
    if (initialize_ncurses() == EXIT_FAILURE) {
        fprintf(stderr, "Error initializing ncurses.\n");
        return EXIT_FAILURE;
    }
    // * Game variables
    WINDOW *win = NULL;
    nodelay(win, TRUE);
    // * Size of the window
    int height = 0, width = 0;
    getmaxyx(stdscr, height, width);
    // * Create the initial window
    win = newwin(height, width, 0, 0);
    // * Draw initial border
    box(win, 0, 0);   // ! Redraw border
    wrefresh(win);
    // * Refresh the screen and window initially
    refresh();
    wrefresh(win);
    // * Size of the grid game
    char grid[GAME_HEIGHT][GAME_WIDTH];
    memset(grid, ' ', sizeof(grid));
    // * Game status: 0=menu, 1=initialization, 2=running, -2=pause, -1=quit
    int status = 0;
    int drone_pos[4] = {0, 0, 0, 0};
    int drone_force[2] = {0, 0};

    // Variabili per il punteggio
    int score = 500000000;
    int distance_traveled = 0;
    int count_obstacles = 0;
    time_t start_time = time(NULL);

    pid_t insp_pid = launch_inspection_window();

    // * Char read from keyboard
    char c;
    fd_set read_keyboard;
    struct timeval timeout;
    do {
        // Se il segnale è stato ricevuto, esegui la scrittura sul logfile
        if (sig_received) {
            // Esegui qui la scrittura in modo "normale"
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            fprintf(logfile, "[%02d:%02d:%02d] PID: %d - %s\n",
                    t->tm_hour, t->tm_min, t->tm_sec, getpid(),
                    "Blackboard is active.");
            fflush(logfile);

            // Reset della variabile per poter rilevare il prossimo segnale
            sig_received = 0;
        }
        switch (status) {
            case 0: { // * Menu
                const char *message = "Press S to start or Q to quit";
                int msg_length = (int)strlen(message);
                mvwprintw(win, height / 2, (width - msg_length) / 2, "%s", message);

                // * Attempt to read a character from the keyboard pipe (non-blocking)
                FD_ZERO(&read_keyboard);
                FD_SET(keyboard, &read_keyboard);

                timeout.tv_sec = 0;
                timeout.tv_usec = 1e6/FRAME_RATE; // * Frame rate of ~60Hz

                if (select(keyboard + 1, &read_keyboard, NULL, NULL, &timeout) > 0) {
                    if (FD_ISSET(keyboard, &read_keyboard)) {
                        const ssize_t bytesRead = read(keyboard, &c, 1);
                        if (bytesRead == -1) {
                            perror("read keyboard");
                            break;
                        }
                    }
                }
                else {
                    c = '\0';
                }
                // * Change the game status
                if (c == 'q') status = -1; // * Then quit
                if (c == 's') {
                    status = 1; // * Run the game
                    werase(win);      // * Erase entire window
                }
                break;
            }
            case 1: { // * initialization
                std::cout << "Starting subscriber." << std::endl;

                CustomTransportSubscriber *mysub = new CustomTransportSubscriber();
                if (mysub->init())
                {
                    mysub->run(grid);

                    // Ora la griglia è stata popolata; ad esempio, la si può stampare
                    for (int y = 0; y < GAME_HEIGHT; y++)
                    {
                        for (int x = 0; x < GAME_WIDTH; x++)
                        {
                            std::cout << grid[y][x];
                        }
                        std::cout << std::endl;
                    }
                }
                delete mysub;

                // ! Clean possible dirties in grid due to pipes
                for (int row = 0; row < GAME_HEIGHT; row++) {
                    for (int col = 0; col < GAME_WIDTH; col++) {
                        if (!strchr("o0123456789", grid[row][col])) {
                            grid[row][col] = ' ';
                        }
                    }
                }
                // Conta il numero di ostacoli
                for (int r = 0; r < GAME_HEIGHT; r++) {
                    for (int col = 0; col < GAME_WIDTH; col++) {
                        if (grid[r][col] == 'o')
                            count_obstacles++;
                    }
                }

                // * Setting drone initial positions
                drone_pos[0] = GAME_WIDTH / 2;
                drone_pos[1] = GAME_HEIGHT / 2;
                drone_pos[2] = GAME_WIDTH / 2;
                drone_pos[3] = GAME_HEIGHT / 2;
                // * Run the game
                status = 2;
                break;
            }
            case 2: { // * Running
                // Salva la posizione precedente per calcolare la velocità
                int prev_x = drone_pos[0], prev_y = drone_pos[1];
                // * Clean the previous position of the drone in the grid
                grid[drone_pos[1]][drone_pos[0]] = ' ';
                // * Draw the new map proportionally to the window dimention
                for (int row = 1; row < GAME_HEIGHT-1; row++) {
                    for (int col = 1; col < GAME_WIDTH-1; col++) {
                        if (grid[row][col] == 'o') {
                            wattron(win, COLOR_PAIR(3)); // * YELLOW for obstacles
                            mvwprintw(win, row * height / GAME_HEIGHT, col * width / GAME_WIDTH, "o");
                            wattroff(win, COLOR_PAIR(3));
                            continue;
                        }
                        if (strchr("0123456789", grid[row][col])) {
                            wattron(win, COLOR_PAIR(2)); // * GREEN for targets
                            mvwprintw(win, row * height / GAME_HEIGHT, col * width / GAME_WIDTH, "%c", grid[row][col]);
                            wattroff(win, COLOR_PAIR(2));
                        }
                    }
                }
                // * Attempt to read a character from the keyboard pipe (non-blocking)
                FD_ZERO(&read_keyboard);
                FD_SET(keyboard, &read_keyboard);

                timeout.tv_sec = 0;
                timeout.tv_usec = 1e6/FRAME_RATE; // * Frame rate of ~60Hz

                if (select(keyboard + 1, &read_keyboard, NULL, NULL, &timeout) > 0) {
                    if (FD_ISSET(keyboard, &read_keyboard)) {
                        const ssize_t bytesRead = read(keyboard, &c, 1);
                        if (bytesRead == -1) {
                            perror("read keyboard");
                            break;
                        }
                    }
                }
                else {
                    c = '\0';
                }
                // * Clean the previous position of the drone in the map and draw the current
                mvwprintw(win, drone_pos[1]*height/GAME_HEIGHT, drone_pos[0]*width/GAME_WIDTH, " ");
                wattron(win, COLOR_PAIR(1)); // * BLUE for drone
                mvwprintw(win, drone_pos[3]*height/GAME_HEIGHT, drone_pos[2]*width/GAME_WIDTH, "+");
                wattroff(win, COLOR_PAIR(1));
                // * Compute the new forces of the drone
                command_drone(drone_force, c);
                // * Send the new grid to drone dynamics
                if (write(dynamic_write, grid, GAME_WIDTH * GAME_HEIGHT * sizeof(char)) == -1) {
                    perror("write target");
                    status = -1;
                    c = 'q';
                    break;
                }
                // * Send drone positions and forces generate by the user
                char msg[100];
                snprintf(msg, sizeof(msg), "%d,%d,%d,%d,%d,%d", drone_pos[0], drone_pos[1], drone_pos[2],
                    drone_pos[3], drone_force[0], drone_force[1]);
                if (write(dynamic_write, msg, sizeof(msg)) == -1) {
                    perror("write");
                    status = -1;
                    c = 'q';
                    break;
                }
                // * Retrieve the new position
                char in_buf[32];
                if (read(dynamic_read, in_buf, sizeof(in_buf)) == -1) {
                    perror("write");
                    status = -1;
                    c = 'q';
                    break;
                }
                drone_pos[0] = drone_pos[2];
                drone_pos[1] = drone_pos[3];
                if (sscanf(in_buf, "%d,%d", &drone_pos[2], &drone_pos[3]) != 2) {
                    perror("sscanf");
                    status = -1;
                    c = 'q';
                    break;
                }
                // Calcola la velocità (ad esempio, come differenza fra posizione nuova e precedente)
                int vel_x = drone_pos[2] - prev_x;
                int vel_y = drone_pos[3] - prev_y;

                // Use the new oversampled function to remove any target along the path
                remove_target_on_path_oversample(grid, prev_x, prev_y, drone_pos[2], drone_pos[3]);

                // Prepara il messaggio con i dati: forza, posizione e velocità
                char insp_msg[128];
                char key;
                if (c == '\0') key = '-';
                else key = c;
                snprintf(insp_msg, sizeof(insp_msg), "%d,%d,%d,%d,%d,%d,%c", drone_force[0], drone_force[1],
                    drone_pos[2], drone_pos[3], vel_x, vel_y, key);
                const int fd = open(INSPECTOR_FIFO, O_WRONLY);
                // Invia il messaggio al pipe per l'inspector
                if (write(fd, insp_msg, strlen(insp_msg)) == -1) {
                    perror("write insp_pipe");
                    status = -1;
                    c = 'q';
                }
                close(fd);
                // Aggiorna la distanza percorsa
                distance_traveled += abs(drone_pos[2] - prev_x) + abs(drone_pos[3] - prev_y);
                // Calcola il tempo trascorso
                int elapsed_time = (int)(time(NULL) - start_time);
                // Conta il numero di target
                int count_targets = 0, count_obstacles = 0;
                for (int r = 0; r < GAME_HEIGHT; r++) {
                    for (int col = 0; col < GAME_WIDTH; col++) {
                        if (strchr("0123456789", grid[r][col]) != NULL)
                            count_targets++;
                    }
                }
                // Calcola il punteggio con una formula ponderata (questa è solo un'ipotesi)
                score -= elapsed_time * 10 + distance_traveled * 5 + count_obstacles/((10 -count_targets) * 3000);
                if (score < 0) score = 0;
                if (count_targets == 0) {
                    status = -1;
                    c = 'q';
                    mvwprintw(win, height/2, width/2, "YOU WIN SCORE %d", score);
                }
                if (score <= 0) {
                    status = -1;
                    c = 'q';
                    mvwprintw(win, height/2, width/2, "GAME OVER");
                }
                break;
            }
        }
        // * See if the windows is resized
        int new_height = 0, new_width = 0;
        getmaxyx(stdscr, new_height, new_width);
        if (!(new_height == height && new_width == width)) {
            height = new_height;
            width = new_width;

            // * Delete old window and create a new one
            delwin(win);
            win = newwin(height, width, 0, 0);
        }
        // * Draw border for new window
        box(win, 0, 0);   // * Redraw border
        // * Stampa il punteggio a posizione y=0, x=4
        mvwprintw(win, 0, 4, "Score: %d", score);
        wrefresh(win);
        refresh();  // * Ensure standard screen updates
        // * Refresh the standard screen and the new window
        wrefresh(win);
        wrefresh(stdscr);
    } while (!(status == -1  && c == 'q')); // * Exit on 'q' and if status is -2
    sleep(4);
    // Prima di uscire, invia un segnale di terminazione all'intero gruppo
    kill(-insp_pid, SIGTERM);
    // Attendi che il processo di inspection termini
    waitpid(insp_pid, NULL, 0);

    // * Final cleanup
    if (win) {
        delwin(win);
    }
    endwin();

    for (int i = 0; i < NUM_CHILD_PIPES; i++) {
        close(read_fds[i]);
    }
    close(write_fds);
    fclose(logfile);

    return EXIT_SUCCESS;
}