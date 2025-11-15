cmake_minimum_required(VERSION 3.20)
project(ic-proj-2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_WARNING_FLAGS -Wall -Wextra -Wpedantic)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

# Force default Release build for faster encoding when using plain 'make'
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Optional OpenCV exercises
find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)
if(OpenCV_FOUND)
  message(STATUS "OpenCV found - building OpenCV exercises")
  add_executable(cv_extract_channel src/cv_extract_channel.cpp)
  target_include_directories(cv_extract_channel PRIVATE ${INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(cv_extract_channel PRIVATE ${OpenCV_LIBS})
  target_compile_options(cv_extract_channel PRIVATE ${COMMON_WARNING_FLAGS})

  add_executable(cv_image_effects
    src/cv_image_effects.cpp
    src/effects/negative.cpp
    src/effects/mirror.cpp
    src/effects/rotate.cpp
    src/effects/brightness.cpp
  )
  target_include_directories(cv_image_effects PRIVATE ${INCLUDE_DIR} ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(cv_image_effects PRIVATE ${OpenCV_LIBS})
  target_compile_options(cv_image_effects PRIVATE ${COMMON_WARNING_FLAGS})
else()
  message(WARNING "OpenCV not found - skipping OpenCV exercises")
endif()

# Golomb library and CLI
add_library(golomb STATIC src/golomb.cpp)
target_include_directories(golomb PUBLIC ${INCLUDE_DIR})
target_compile_options(golomb PRIVATE ${COMMON_WARNING_FLAGS})

add_executable(golomb_main src/golomb_main.cpp)
target_include_directories(golomb_main PRIVATE ${INCLUDE_DIR})
target_link_libraries(golomb_main PRIVATE golomb)
target_compile_options(golomb_main PRIVATE ${COMMON_WARNING_FLAGS})

# Add bit_stream library from P/01
set(BIT_STREAM_DIR "${CMAKE_SOURCE_DIR}/../01/bit_stream/src")
add_library(bit_stream STATIC
  ${BIT_STREAM_DIR}/bit_stream.cpp
  ${BIT_STREAM_DIR}/byte_stream.cpp
)
target_include_directories(bit_stream PUBLIC ${BIT_STREAM_DIR})

# Lossless audio codec (requires SndFile + bit_stream)
find_package(SndFile REQUIRED)
add_executable(lossless_codec src/lossless_codec_main.cpp src/lossless_codec.cpp)
target_include_directories(lossless_codec PRIVATE ${INCLUDE_DIR} ${BIT_STREAM_DIR})
target_link_libraries(lossless_codec PRIVATE SndFile::sndfile golomb bit_stream)
target_compile_options(lossless_codec PRIVATE ${COMMON_WARNING_FLAGS})
