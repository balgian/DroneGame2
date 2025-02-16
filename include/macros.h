//
// Created by Gian Marco Balia
//
// macros.h
#ifndef MACROS_H
#define MACROS_H

/*
* - Keyboard input manager (write to Blackboard -> 1 pipe)
* - Obstacles generator (write to Targets -> 1 pipe)
* - Targets generator (read from Obstacles -> same pipe of Obstacles)
* - Drone dynamics process (read & write from/to Blackboard-> 2 pipes)
*
* Total: 4 pipes
*/
#define NUM_CHILD_PIPES 4
#define NUM_CHILD_PROCESSES 6

#define INSPECTOR_FIFO "/tmp/inspector_fifo"

// * Obstacles Server
#define TCP_LISTENING_PORT_OBSTACLES 12345
#define IPV4_OBSTACLES_SERVER "127.0.0.1"
#define TOPIC_NAME_OBSTACLES "topic 1"

// * Targets Server
#define TCP_LISTENING_PORT_TARGETS 12346
#define IPV4_TARGETS_SERVER "127.0.0.1"
#define TOPIC_NAME_TARGETS "topic 2"

// * Blackboard Client
#define SERVER_PORT_OBSTACLES 12345         // ! <-TCP_LISTENING_PORT_OBSTACLES
#define SERVER_PORT_TARGETS 12346           // ! <-TCP_LISTENING_PORT_TARGETS
#define IPV4_OBSTACLES_CLIENT "127.0.0.1"   // ! <-IPV4_OBSTACLES_SERVER
#define IPV4_TARGETS_CLIENT "127.0.0.1"     // ! <-IPV4_TARGETS_SERVER


// * Game parameters
#define GAME_HEIGHT 100
#define GAME_WIDTH 100
#define FRAME_RATE 60.0                     // * Hz

#define INSPECT_WIDTH 20

// * Physic parameters
#define DRONE_MASS 1.0
#define DAMPING 1.0
#define TIME 10.0
// * Obstacles' repulsive force
#define ETA 3.0                             // * Repulsion scaling factor
#define RHO_OBST 6.0                        // * Influence distance for repulsion
#define MIN_RHO_OBST 4.0                    // * Minimum distance of repulsion
// * Targets' attractive force
#define EPSILON 1.0                         // * Attractive scaling factor
#define RHO_TRG 6.0                         // * Influence distance for attraction
#define MIN_RHO_TRG 4.0                     // * Minimum distance of attraction

#define MAX_SCORE 500000000                 // * Maximum game score

#endif                                      // MACROS_H