cmake_minimum_required(VERSION 3.22)
project(coin_selection_c C CXX)


# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra -lm")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra -lm")

# Include the header files
include_directories(include)
include_directories(external/cjson)

# Add cJSON source files to the build
set(CJSON_SOURCE_FILES src/cjson/cJSON.c)

#[===[
# Compile .c files as C++
set(qpid_dispatch_SOURCES
    src/main.c
    src/fee.c
    src/parser.c
    src/generator.c
    src/coin_selection.c
    src/simulation.c
    src/user.c
)

set_source_files_properties(${qpid_dispatch_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(qpid-dispatch OBJECT ${qpid_dispatch_SOURCES})
]===]

# Define the sources for the main executable
set(PROJECT_SOURCE_FILES
        src/main.cpp
        src/fee.cpp
        src/parser.cpp
        src/generator.cpp
        src/coin_selection.cpp
        src/simulation.cpp
        src/user.cpp
        ${CJSON_SOURCE_FILES}
)

add_executable(coin_selection_c ${PROJECT_SOURCE_FILES})
