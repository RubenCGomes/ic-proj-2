cmake_minimum_required(VERSION 3.20)
project(ic-proj-2)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define _GNU_SOURCE
add_definitions(-D_GNU_SOURCE)

# Set the output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Add include directories
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
include_directories(${INCLUDE_DIR})

# Try to find OpenCV package (only core modules needed)
find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)

# Only build OpenCV exercises if OpenCV is found
if(OpenCV_FOUND)
    message(STATUS "OpenCV found - Building OpenCV exercises")
    include_directories(${OpenCV_INCLUDE_DIRS})

    # Exercise 1: Channel Extractor (single file with inline function in header)
    add_executable(cv_extract_channel
        src/cv_extract_channel.cpp
    )
    target_include_directories(cv_extract_channel PUBLIC ${INCLUDE_DIR})
    target_link_libraries(cv_extract_channel ${OpenCV_LIBS})
    target_compile_options(cv_extract_channel PRIVATE -Wall -Wextra -Wpedantic)

    # Exercise 2: Image Effects (separate implementation files)
    add_executable(cv_image_effects
        src/cv_image_effects.cpp
        src/effects/negative.cpp
        src/effects/mirror.cpp
        src/effects/rotate.cpp
        src/effects/brightness.cpp
    )
    target_include_directories(cv_image_effects PUBLIC ${INCLUDE_DIR})
    target_link_libraries(cv_image_effects ${OpenCV_LIBS})
    target_compile_options(cv_image_effects PRIVATE -Wall -Wextra -Wpedantic)
else()
    message(WARNING "OpenCV not found - Skipping OpenCV exercises (cv_extract_channel, cv_image_effects)")
endif()

# Golomb main program
add_executable(golomb_main
        src/golomb_main.cpp
        src/golomb.cpp
)

# Add other executables here (non-OpenCV dependent)
# Example:
# add_executable(other_program src/other_program.cpp)
# target_compile_options(other_program PRIVATE -Wall -Wextra -Wpedantic)
