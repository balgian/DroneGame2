//
// Created by Gian Marco Balia
//
// src/targets_generator.cpp
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

#include "TargetsPubSubTypes.hpp"
#include "macros.h"

using namespace eprosima::fastdds::dds;
using namespace eprosima::fastdds::rtps;
using namespace std::chrono_literals;

// Puntatore globale al file di log
FILE *logfile;
static volatile sig_atomic_t keep_running = 1;

class CustomTargetsPublisher {
private:
    // * DDS Message (defined in Targets.idl)
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
                //std::cout << "Publisher unmatched." << std::endl;
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

    bool init() {
        my_message_.targets_number(0);

        DomainParticipantQos participantQos = PARTICIPANT_QOS_DEFAULT;

        //participantQos.name("Targets_Publisher");

        // * Configure the current participant as SERVER
        participantQos.wire_protocol().builtin.discovery_config.discoveryProtocol = DiscoveryProtocol::SERVER;

        // * Add custom user transport with TCP port TCP_LISTENING_PORT_TARGETS
        auto data_transport = std::make_shared<TCPv4TransportDescriptor>();
        data_transport->add_listener_port(TCP_LISTENING_PORT_TARGETS);
        participantQos.transport().user_transports.push_back(data_transport);

        // * Define the listening locator to be on interface IPV4_TARGETS_SERVER and port TCP_LISTENING_PORT_TARGETS
        constexpr uint16_t tcp_listening_port = TCP_LISTENING_PORT_TARGETS;
        Locator_t listening_locator;
        IPLocator::setIPv4(listening_locator, IPV4_TARGETS_SERVER);
        IPLocator::setPhysicalPort(listening_locator, tcp_listening_port);
        IPLocator::setLogicalPort(listening_locator, tcp_listening_port);
        participantQos.wire_protocol().builtin.metatrafficUnicastLocatorList.push_back(listening_locator);

        participant_ = DomainParticipantFactory::get_instance()->create_participant(0, participantQos);
        if (participant_ == nullptr) {
            std::cerr << "Failed to create DomainParticipant with TCP/Discovery configuration in Targets generator" << std::endl;
            return false;
        }

        type_.register_type(participant_, "Targets");
        topic_ = participant_->create_topic(TOPIC_NAME_TARGETS, "Targets", TOPIC_QOS_DEFAULT);
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
        // * Clear the previous sequeces
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
        while (!flag || keep_running) {
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

    void run(int read_fd) {
        srand(static_cast<unsigned int>(time(NULL)));
        while (keep_running) {
            char grid[GAME_HEIGHT][GAME_WIDTH];
            memset(grid, ' ', GAME_HEIGHT*GAME_WIDTH);
            if (read(read_fd, grid, GAME_HEIGHT * GAME_WIDTH * sizeof(char)) == -1) {
                perror("read");
                EXIT_FAILURE;
            }
            // * Generate targets (decreasing from '9' to '0')
            char num_target = '9';
            while (num_target >= '0') {
                int x = (rand() % (GAME_WIDTH - 2)) + 1;
                int y = (rand() % (GAME_HEIGHT - 2)) + 1;
                // * Excise the center of the map (the drone will be there at the beginning)
                if (grid[y][x] == ' ' && !(x == GAME_WIDTH / 2 && y == GAME_HEIGHT / 2)) {
                    grid[y][x] = num_target;
                    num_target--;
                }
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
    fprintf(logfile, "[%02d:%02d:%02d] PID: %d - Targets is active.\n",
            t->tm_hour, t->tm_min, t->tm_sec, getpid());
    fflush(logfile);
}

int main (int argc, char *argv[]) {
    /*
     * Targets process
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

    CustomTargetsPublisher* mypub = new CustomTargetsPublisher();
    if (mypub->init()) {
        mypub->run(read_fd);
    } else {
        std::cerr << "Publisher initialization failed." << std::endl;
    }
    delete mypub;
    close(read_fd);

    return 0;
}