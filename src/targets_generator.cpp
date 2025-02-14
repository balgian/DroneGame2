//
// Created by Gian Marco Balia
//
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.hpp>
#include <fastdds/utils/IPLocator.hpp>

#include "TargetsPubSubTypes.hpp"  // Tipo DDS generato da Targets.idl
#include "macros.h"                // Deve definire GAME_HEIGHT e GAME_WIDTH

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;
using namespace std::chrono_literals;

// Puntatore globale al file di log
FILE* logfile;

// Classe per il publisher DDS per Targets
class CustomTargetsPublisher {
private:
    // Messaggio DDS (tipo Targets definito in Targets.idl)
    Targets my_message_;
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
    CustomTargetsPublisher()
        : participant_(nullptr)
        , publisher_(nullptr)
        , topic_(nullptr)
        , writer_(nullptr)
        , type_(new TargetsPubSubType())
    { }

    virtual ~CustomTargetsPublisher() {
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
        my_message_.targets_number(0);

        DomainParticipantQos participantQos;
        participantQos.name("Participant_publisher");
        /*
        // Disable built-in transports
        participantQos.transport().use_builtin_transports = false;

        // Create and configure the TCP transport.
        // Use new with std::shared_ptr if needed.
        std::shared_ptr<TCPv4TransportDescriptor> tcp_transport(
            new TCPv4TransportDescriptor());
        tcp_transport->add_listener_port(5100);
        // Set the WAN address (public address) and specify local interface
        tcp_transport->set_WAN_address("127.0.0.1");
        tcp_transport->interfaceWhiteList.push_back("127.0.0.1");
        //participantQos.transport().user_transports.push_back(tcp_transport);

        // Configure discovery in SERVER mode:
        //participantQos.wire_protocol().builtin.discovery_config.use_SIMPLE_EndpointDiscoveryProtocol = false;
        //participantQos.wire_protocol().builtin.discovery_config.discoveryProtocol = eprosima::fastdds::rtps::DiscoveryProtocol::SERVER;
        // Instead of using m_ServerListeningAddresses (which is not supported),
        // set a locator into m_DiscoveryServers.
        Locator_t server_locator;
        IPLocator::setIPv4(server_locator, 127, 0, 0, 1);
        server_locator.port = 11811;
        //participantQos.wire_protocol().builtin.discovery_config.m_DiscoveryServers.push_back(server_locator);
        */
        participant_ = DomainParticipantFactory::get_instance()->create_participant(1, participantQos);
        if (participant_ == nullptr) {
            std::cerr << "Failed to create DomainParticipant with TCP/Discovery configuration in Targets generator" << std::endl;
            return false;
        }

        type_.register_type(participant_, "Targets");
        topic_ = participant_->create_topic("topic 2", "Targets", TOPIC_QOS_DEFAULT);
        if (topic_ == nullptr) {
            return false;
        }

        publisher_ = participant_->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr);
        if (publisher_ == nullptr) {
            return false;
        }

        writer_ = publisher_->create_datawriter(topic_, DATAWRITER_QOS_DEFAULT, &listener_);
        if (writer_ == nullptr) {
            return false;
        }
        return true;
    }

    //! Funzione publish_from_grid:
    //! Scansiona la griglia (GAME_HEIGHT x GAME_WIDTH) per individuare celle non vuote (diverse da ' ')
    //! e compila il messaggio DDS con le coordinate dei target e il loro numero.
    bool publish_from_grid(const char grid[GAME_HEIGHT][GAME_WIDTH]) {
        // Pulisce le sequenze precedenti
        my_message_.targets_x().clear();
        my_message_.targets_y().clear();
        int count = 0;
        for (int r = 0; r < GAME_HEIGHT; r++) {
            for (int c = 0; c < GAME_WIDTH; c++) {
                if ((grid[r][c] >= '0' && grid[r][c] <= '9')) {
                    my_message_.targets_x().push_back(c);
                    my_message_.targets_y().push_back(r);
                    count++;
                }
            }
        }
        my_message_.targets_number(count);

        int flag = 0;
        while (!flag) {
            if (listener_.matched_ > 0) {
                writer_->write(&my_message_);
                eprosima::fastdds::dds::Duration_t timeout;
                timeout.seconds = 5;
                timeout.nanosec = 0;
                writer_->wait_for_acknowledgments(timeout);
                flag = 1;
            }
        }
        return true;
    }

    void run(char grid[GAME_HEIGHT][GAME_WIDTH]) {
        // Genera i target: inserisce cifre decrescenti da '9' a '0'
        srand(static_cast<unsigned int>(time(NULL)));
        char num_target = '9';
        while (num_target > '0') {
            int x = (rand() % (GAME_WIDTH - 2)) + 1;
            int y = (rand() % (GAME_HEIGHT - 2)) + 1;
            // Se la cella è vuota e non è il centro, inserisce il target
            if (grid[y][x] == ' ' && !(x == GAME_WIDTH / 2 && y == GAME_HEIGHT / 2)) {
                grid[y][x] = num_target;
                num_target--;
            }
        }
        publish_from_grid(grid);
    }
};

void signal_triggered(int signum) {
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    fprintf(logfile, "[%02d:%02d:%02d] PID: %d - Targets is active.\n",
            t->tm_hour, t->tm_min, t->tm_sec, getpid());
    fflush(logfile);
}

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

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <read_fd> <logfile_fd>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int read_fd = atoi(argv[1]);
    if (read_fd <= 0) {
        fprintf(stderr, "Invalid read file descriptor: %s\n", argv[0]);
        return EXIT_FAILURE;
    }

    int logfile_fd = atoi(argv[2]);
    logfile = fdopen(logfile_fd, "a");
    if (!logfile) {
        perror("fdopen logfile");
        return EXIT_FAILURE;
    }

    char grid[GAME_HEIGHT][GAME_WIDTH];
    memset(grid, ' ', GAME_HEIGHT*GAME_WIDTH);
    if (read(read_fd, grid, GAME_HEIGHT * GAME_WIDTH * sizeof(char)) == -1) {
        perror("read");
        return EXIT_FAILURE;
    }
    CustomTargetsPublisher* mypub = new CustomTargetsPublisher();
    if (mypub->init()) {
        mypub->run(grid);
    } else {
        std::cerr << "Publisher initialization failed." << std::endl;
    }
    delete mypub;

    close(read_fd);

    return 0;
}