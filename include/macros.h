// macros.h
#ifndef MACROS_H
#define MACROS_H

/*
* - Keyboard input manager (write to Blackboard -> 1 pipe)
* - Obstacle generator (read & write -> 2 pipes)
* - Target generators (read & write -> 2 pipes)
* - Drone dynamics process (read & write -> 2 pipes)
*
* Total: 7 pipes
*/
#define NUM_CHILD_PIPES 4
#define NUM_CHILD_PROCESSES 6

#define INSPECTOR_FIFO "/tmp/inspector_fifo"

// * Game parameters
#define GAME_HEIGHT 100
#define GAME_WIDTH 100
#define FRAME_RATE 60.0 // * Hz

#define INSPECT_WIDTH 20

// * Physic parameters
#define DRONE_MASS 1.0
#define DAMPING 1.0
#define TIME 10.0
// * Obstacles' repulsive force
#define ETA 3.0  // * Repulsion scaling factor
#define RHO_OBST 8.0  // * Influence distance for repulsion
#define MIN_RHO_OBST 4.0 // * Minimum distance of repulsion
// * Targets' attractive force
#define EPSILON 3.0 // * Attractive scaling factor
#define RHO_TRG 8.0 // * Influence distance for attraction
#define MIN_RHO_TRG 4.0 // * Minimum distance of attraction

#endif // MACROS_H