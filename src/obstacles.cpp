//
// Created by Gian Marco Balia
//
// src/obstacles.cpp

#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <ctime>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include "ObstaclesPubSubTypes.hpp"  // Tipo DDS generato da Obstacles.idl
#include "macros.h"                  // Deve definire GAME_HEIGHT e GAME_WIDTH

using namespace eprosima::fastdds::dds;
using namespace std::chrono_literals;

FILE* logfile;

class CustomTransportPublisher {
private:
    // Messaggio DDS (tipo Obstacles definito in Obstacles.idl)
    Obstacles my_message_;
    DomainParticipant* participant_;
    Publisher* publisher_;
    Topic* topic_;
    DataWriter* writer_;
    TypeSupport type_;

    class PubListener : public DataWriterListener {
    public:
        std::atomic_int matched_;
        PubListener() : matched_(0) {}
        ~PubListener() override {}

        void on_publication_matched(DataWriter* writer, const PublicationMatchedStatus& info) override {
            if (info.current_count_change == 1) {
                matched_ = info.total_count;
                std::cout << "Publisher matched." << std::endl;
                // Eventuale stampa dei trasporti utilizzati può essere aggiunta qui
            } else if (info.current_count_change == -1) {
                matched_ = info.total_count;
                std::cout << "Publisher unmatched." << std::endl;
            } else {
                std::cout << info.current_count_change
                          << " is not a valid value for PublicationMatchedStatus current count change." << std::endl;
            }
        }
    } listener_;

public:
    CustomTransportPublisher()
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new ObstaclesPubSubType())
    { }

    virtual ~CustomTransportPublisher() {
        if (writer_ != nullptr) {
            publisher_->delete_datawriter(writer_);
        }
        if (publisher_ != nullptr) {
            participant_->delete_publisher(publisher_);
        }
        if (topic_ != nullptr) {
            participant_->delete_topic(topic_);
        }
        DomainParticipantFactory::get_instance()->delete_participant(participant_);
    }

    //! Inizializza il publisher DDS
    bool init() {
        // Imposta un valore iniziale (eventualmente 0) per il campo obstacles_number
        my_message_.obstacles_number(0);

        DomainParticipantQos participantQos;
        participantQos.name("Participant_publisher");

        // // Explicit configuration of shm transport
        // participantQos.transport().use_builtin_transports = false;
        // auto shm_transport = std::make_shared<SharedMemTransportDescriptor>();
        // shm_transport->segment_size(10 * 1024 * 1024);
        // participantQos.transport().user_transports.push_back(shm_transport);

        participant_ = DomainParticipantFactory::get_instance()->create_participant(1, participantQos);
        if (participant_ == nullptr) {
            return false;
        }

        // Registra il tipo
        type_.register_type(participant_);

        // Create the publications Topic
        topic_ = participant_->create_topic("topic 1", type_.get_type_name(), TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) {
            return false;
        }

        // Create the Publisher
        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
        if (publisher_ == nullptr) {
            return false;
        }

        // Create the DataWriter
        writer_ = publisher_->create_datawriter(topic_, DATAWRITER_QOS_DEFAULT, &listener_);
        if (writer_ == nullptr) {
            return false;
        }
        return true;
    }

    // ! Funzione publish_from_grid:
    // ! Scansiona la griglia (GAME_HEIGHT x GAME_WIDTH) per individuare celle contrassegnate con 'o'
    //! e compila il messaggio DDS con le coordinate degli ostacoli e il loro numero.
    bool publish_from_grid(const char grid[GAME_HEIGHT][GAME_WIDTH]) {
        // Pulisce le sequenze precedenti
        my_message_.obstacles_x().clear();
        my_message_.obstacles_y().clear();
        int count = 0;
        for (int r = 0; r < GAME_HEIGHT; r++) {
            for (int c = 0; c < GAME_WIDTH; c++) {
                if (grid[r][c] == 'o') {
                    my_message_.obstacles_x().push_back(c);
                    my_message_.obstacles_y().push_back(r);
                    count++;
                }
            }
        }
        my_message_.obstacles_number(count);
        if (listener_.matched_ > 0) {
            writer_->write(&my_message_);
            return true;
        }
        return false;
    }

    //! Funzione run:
    //! Per il numero di campioni specificato, genera una griglia e crea ostacoli in essa
    //! (usando la stessa logica di obstacles.c) e pubblica il messaggio DDS tramite publish_from_grid().
    void run(uint32_t total_obstacles) {
        // Inizializza il seme per i numeri casuali
        srand(static_cast<unsigned int>(time(NULL)));
        // Crea una griglia GAME_HEIGHT x GAME_WIDTH inizializzata a ' '
        char grid[GAME_HEIGHT][GAME_WIDTH];
        memset(grid, ' ', sizeof(grid));

        while (total_obstacles > 0) {
            // Genera coordinate casuali (escludendo i bordi)
            int x = (rand() % (GAME_WIDTH - 2)) + 1;
            int y = (rand() % (GAME_HEIGHT - 2)) + 1;
            // Se la cella è vuota e non è il centro, posiziona un ostacolo ('o')
            if (grid[y][x] == ' ' && !(x == GAME_WIDTH / 2 && y == GAME_HEIGHT / 2)) {
                grid[y][x] = 'o';
                total_obstacles--;
            }
        }

        // Pubblica il messaggio DDS basato sulla griglia generata
        if (publish_from_grid(grid)) {
            std::cout << "Obstacles message published." << std::endl;
        } else {
            std::cout << "Publishing failed (no subscribers or no obstacles)." << std::endl;
        }
    }
};

void signal_triggered(int signum) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(logfile, "[%02d:%02d:%02d] PID: %d - %s\n", t->tm_hour, t->tm_min, t->tm_sec, getpid(),
        "Obstacles is active.");
    fflush(logfile);
}

// La main rimane semplice come da specifica
int main(int argc, char* argv[]) {
    // * Imposta il gestore per SIGUSR1
    struct sigaction sa1;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = signal_triggered;
    sa1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // * Verifica i parametri: ci aspettiamo 2 argomenti oltre al nome del programma
    //    (Usage: <program> <write_fd> <logfile_fd>)
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <read_fd> <write_fd> <logfile_fd>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // * Parsing del file descriptor per la scrittura (ad esempio, per comunicare con altri processi)
    int write_fd = atoi(argv[1]);
    if (write_fd <= 0)
    {
        fprintf(stderr, "Invalid write file descriptor: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    // * Parsing del file descriptor per il file di log e apertura del file in modalità append
    int logfile_fd = atoi(argv[2]);
    logfile = fdopen(logfile_fd, "a");
    if (!logfile)
    {
        perror("fdopen logfile");
        return EXIT_FAILURE;
    }

    std::cout << "Starting publisher." << std::endl;
    // Calcola il numero totale di ostacoli da inviare in base alla dimensione della griglia
    uint32_t total_obstacles = static_cast<int>(GAME_HEIGHT * GAME_WIDTH * 0.001);

    CustomTransportPublisher* mypub = new CustomTransportPublisher();
    if (mypub->init()) {
        mypub->run(total_obstacles);
    }

    delete mypub;
    return 0;
}