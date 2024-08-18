cmake_minimum_required(VERSION 3.22)
project(coin_selection_c C)

set(CMAKE_C_STANDARD 17)
# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_DEBUG "-O0 -g -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O3 -Wall -Wextra")

# Include the header files
include_directories(include)
include_directories(external/cjson)

# Add cJSON source files to the build
set(CJSON_SOURCE_FILES src/cjson/cJSON.c)

# Define the sources for the main executable
set(PROJECT_SOURCE_FILES
        src/main.c
        src/fee.c
        src/parser.c
        src/generator.c
        src/coin_selection.c
        src/simulation.c
        src/user.c
        ${CJSON_SOURCE_FILES}
)

add_executable(coin_selection_c ${PROJECT_SOURCE_FILES})
