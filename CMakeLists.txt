cmake_minimum_required(VERSION 3.28)
project(DroneGame C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

# * Include packages
find_package(Curses REQUIRED)

# * Include directories
include_directories(${CURSES_INCLUDE_DIR})
include_directories(include)
# Specifica il percorso degli header di Fast-DDS; assicurati che questo percorso sia corretto
include_directories(/home/parallels/fastdds/include)

# --- Automatic generation of DDS files from Obstacles.idl and Targets.idl ---
set(IDL_OBSTACLES ${CMAKE_CURRENT_SOURCE_DIR}/idl/Obstacles.idl)
set(IDL_TARGET ${CMAKE_CURRENT_SOURCE_DIR}/idl/Targets.idl)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Generate DDS files for Obstacles
add_custom_command(
    OUTPUT
        ${GENERATED_DIR}/ObstaclesPubSubTypes.h
        ${GENERATED_DIR}/ObstaclesPubSubTypes.cxx
        ${GENERATED_DIR}/ObstaclesTypeObjectSupport.cxx   # Supporto agli xtypes per Obstacles
    COMMAND fastddsgen ${IDL_OBSTACLES} -d ${GENERATED_DIR}
    DEPENDS ${IDL_OBSTACLES}
    COMMENT "Generating DDS files from ${IDL_OBSTACLES}"
)

# Generate DDS files for Targets
add_custom_command(
    OUTPUT
        ${GENERATED_DIR}/TargetsPubSubTypes.h
        ${GENERATED_DIR}/TargetsPubSubTypes.cxx
        ${GENERATED_DIR}/TargetsTypeObjectSupport.cxx   # Supporto agli xtypes per Targets
    COMMAND fastddsgen ${IDL_TARGET} -d ${GENERATED_DIR}
    DEPENDS ${IDL_TARGET}
    COMMENT "Generating DDS files from ${IDL_TARGET}"
)

# Custom target that depends on both DDS-generation outputs
add_custom_target(generate_dds_files ALL
    DEPENDS
        ${GENERATED_DIR}/ObstaclesPubSubTypes.h
        ${GENERATED_DIR}/ObstaclesPubSubTypes.cxx
        ${GENERATED_DIR}/ObstaclesTypeObjectSupport.cxx
        ${GENERATED_DIR}/TargetsPubSubTypes.h
        ${GENERATED_DIR}/TargetsPubSubTypes.cxx
        ${GENERATED_DIR}/TargetsTypeObjectSupport.cxx
)

# Add the generated directory to the include paths
include_directories(${GENERATED_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/idl)

# Specifica la directory dove sono presenti le librerie di Fast-DDS
link_directories(/home/parallels/fastdds/lib)

# * Add the executables
add_executable(DroneGame main.c)

# Updated blackboard target: include the generated TypeObjectSupport sources
add_executable(blackboard
    src/blackboard.cpp
    ${GENERATED_DIR}/ObstaclesPubSubTypes.cxx
    ${GENERATED_DIR}/ObstaclesTypeObjectSupport.cxx
    ${GENERATED_DIR}/TargetsPubSubTypes.cxx
    ${GENERATED_DIR}/TargetsTypeObjectSupport.cxx
)

add_executable(keyboard_manager src/keyboard_manager.c)

# For processes using Fast-DDS, compile as C++
add_executable(obstacles
    src/obstacles.cpp
    ${GENERATED_DIR}/ObstaclesPubSubTypes.cxx
    ${GENERATED_DIR}/ObstaclesTypeObjectSupport.cxx
)

# Eseguibile per il publisher dei Targets
add_executable(targets_generator
    src/targets_generator.cpp
    ${GENERATED_DIR}/TargetsPubSubTypes.cxx
    ${GENERATED_DIR}/TargetsTypeObjectSupport.cxx
)

add_executable(drone_dynamics src/drone_dynamics.c)
add_executable(watchdog src/watchdog.c)
add_executable(inspector src/inspector_window.c)

# Make DDS-dependent targets depend on DDS generation
add_dependencies(blackboard generate_dds_files)
add_dependencies(obstacles generate_dds_files)
add_dependencies(targets_generator generate_dds_files)

# Put all executables in the same folder
set_target_properties(
    DroneGame blackboard keyboard_manager obstacles targets_generator drone_dynamics watchdog inspector
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# Link ncurses to the appropriate targets
target_link_libraries(blackboard PRIVATE fastdds fastcdr m ${CURSES_LIBRARIES})
target_link_libraries(keyboard_manager PRIVATE ${CURSES_LIBRARIES})
target_link_libraries(inspector PRIVATE ${CURSES_LIBRARIES})
target_link_libraries(drone_dynamics PRIVATE m)

# Link Fast-DDS and fastcdr for the DDS-using targets
target_link_libraries(obstacles PRIVATE fastdds fastcdr)
target_link_libraries(targets_generator PRIVATE fastdds fastcdr)