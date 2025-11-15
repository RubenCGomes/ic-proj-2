cmake_minimum_required(VERSION 3.16)
project(bit_stream)

SET (CMAKE_BUILD_TYPE "Release")
#SET (CMAKE_BUILD_TYPE "Debug")

SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++20")
SET (CMAKE_CXX_FLAGS_RELEASE "-O3")
SET (CMAKE_CXX_FLAGS_DEBUG "-g3 -fsanitize=address")

# Set binary output directory to ../bin (relative to src)
SET (BASE_DIR ${CMAKE_SOURCE_DIR})
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BASE_DIR}/../bin)

# Create bin directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Find required libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)

# Create common library with bit_stream and byte_stream
add_library(BitStreamLib STATIC bit_stream.cpp byte_stream.cpp)

# Utility executables
add_executable(text2bin text2bin.cpp)
target_link_libraries(text2bin BitStreamLib)

add_executable(bin2text bin2text.cpp)
target_link_libraries(bin2text BitStreamLib)

# Lossy codec executables
add_executable(lossy_codec_enc lossy_codec_enc.cpp)
target_link_libraries(lossy_codec_enc BitStreamLib ${SNDFILE_LIBRARIES})
target_include_directories(lossy_codec_enc PRIVATE ${SNDFILE_INCLUDE_DIRS})
target_compile_options(lossy_codec_enc PRIVATE ${SNDFILE_CFLAGS_OTHER})

add_executable(lossy_codec_dec lossy_codec_dec.cpp)
target_link_libraries(lossy_codec_dec BitStreamLib ${SNDFILE_LIBRARIES})
target_include_directories(lossy_codec_dec PRIVATE ${SNDFILE_INCLUDE_DIRS})
target_compile_options(lossy_codec_dec PRIVATE ${SNDFILE_CFLAGS_OTHER})

