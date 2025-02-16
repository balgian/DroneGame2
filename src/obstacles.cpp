//
// Created by Gian Marco Balia
//
// src/obstacles.cpp

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <ctime>
#include "macros.h"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/rtps/transport/TCPv4TransportDescriptor.hpp>
#include <fastdds/utils/IPLocator.hpp>
#include "ObstaclesPubSubTypes.hpp"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;
using namespace std::chrono_literals;

FILE* logfile;
static volatile sig_atomic_t keep_running = 1;

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
            } else if (info.current_count_change == -1) {
                matched_ = info.total_count;
                //std::cout << "Publisher unmatched." << std::endl;
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
        my_message_.obstacles_number(0);

        DomainParticipantQos participantQos = PARTICIPANT_QOS_DEFAULT;
        participantQos.name("Obstacles_publisher");

        // * Configure the current participant as SERVER
        participantQos.wire_protocol().builtin.discovery_config.discoveryProtocol = DiscoveryProtocol::CLIENT;

        // * Add custom user transport with TCP port 0 (automatic port assignation)
        auto data_transport = std::make_shared<TCPv4TransportDescriptor>();
        data_transport->add_listener_port(0);
        participantQos.transport().user_transports.push_back(data_transport);

        // * Define the server locator to be on interface 192.168.10.57 and port 12346
        constexpr uint16_t server_port = 12346;
        Locator_t server_locator;
        IPLocator::setIPv4(server_locator, "192.168.10.57");
        IPLocator::setPhysicalPort(server_locator, server_port);
        IPLocator::setLogicalPort(server_locator, server_port);

        // *Add the server
        participantQos.wire_protocol().builtin.discovery_config.m_DiscoveryServers.push_back(server_locator);

        participant_ = DomainParticipantFactory::get_instance()->create_participant(1, participantQos);
        if (participant_ == nullptr) {
            std::cerr << "Failed to create DomainParticipant with TCP/Discovery configuration in Obstacles generator" << std::endl;
            return false;
        }

        type_.register_type(participant_, "Obstacles");
        topic_ = participant_->create_topic("topic 1", "Obstacles", TOPIC_QOS_DEFAULT);
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

        int flag = 0;
        while (!flag) {
            if (listener_.matched_ > 0) {
                writer_->write(&my_message_);
                Duration_t timeout;
                timeout.seconds = 5;
                timeout.nanosec = 0;
                writer_->wait_for_acknowledgments(timeout);
                flag = 1;
            }
        }
        return true;
    }

    void run(uint32_t total_obstacles, int write_fd) {
        std::cout << "Obstacles" << std::endl;
        srand(static_cast<unsigned int>(time(NULL)));
        while (keep_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            char grid[GAME_HEIGHT][GAME_WIDTH];
            memset(grid, ' ', sizeof(grid));
            while (total_obstacles > 0) {
                int x = (rand() % (GAME_WIDTH - 2)) + 1;
                int y = (rand() % (GAME_HEIGHT - 2)) + 1;
                if (grid[y][x] == ' ' && !(x == GAME_WIDTH / 2 && y == GAME_HEIGHT / 2)) {
                    grid[y][x] = 'o';
                    total_obstacles--;
                }
            }
            if (write(write_fd, grid, GAME_HEIGHT * GAME_WIDTH * sizeof(char)) == -1) {
                perror("write");
                EXIT_FAILURE;
            }
            publish_from_grid(grid);
        }
    }
};

void signal_close(int signum) {
    keep_running = 0;
}

void signal_triggered(int signum) {
    time_t now = time(NULL);
    tm *t = localtime(&now);
    fprintf(logfile, "[%02d:%02d:%02d] PID: %d - %s\n", t->tm_hour, t->tm_min, t->tm_sec, getpid(),
        "Obstacles is active.");
    fflush(logfile);
}

int main (int argc, char *argv[]) {
    /*
     * Obstacles process
     * @param argv[1]: Read file descriptors
     * @param argv[2]: Write file descriptors
    */
    // * Signal handler closure
    struct sigaction sa0;
    memset(&sa0, 0, sizeof(sa0));
    sa0.sa_handler = signal_close;
    sa0.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM, &sa0, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    // * Signal handler
    struct sigaction sa1;
    memset(&sa1, 0, sizeof(sa1));
    sa1.sa_handler = signal_triggered;
    sa1.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <write_fd> <logfile_fd>\n", argv[0]);
        return EXIT_FAILURE;
    }
    // * Parse the writer (to targets)
    int write_fd = atoi(argv[1]);
    if (write_fd <= 0)
    {
        fprintf(stderr, "Invalid write file descriptor: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    // * Parse the logfile file descriptor
    int logfile_fd = atoi(argv[2]);
    logfile = fdopen(logfile_fd, "a");
    if (!logfile)
    {
        perror("fdopen logfile");
        return EXIT_FAILURE;
    }
    // * Initialise and call the DDS server class paasing the random number of obstacles
    uint32_t total_obstacles = static_cast<int>(GAME_HEIGHT * GAME_WIDTH * 0.001);
    auto* mypub = new CustomTransportPublisher();
    if (mypub->init()) {
        mypub->run(total_obstacles, write_fd);
    }


    delete mypub;
    return 0;
}