#
# CMAKE file for PUCKO - this will create a platform agnostic build
# Mainly for running tests
# NOTE: Not everything get's compiled and the platform stuff is just a mock
#

# set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
cmake_minimum_required(VERSION 2.8)


option(LOGGER_HAVE_NEWLINE "Append newline to strings" ON)
option(LOGGER_HAVE_PTHREADS "Thread saftey" ON)
option(LOGGER_HAVE_SERIAL "Serial log sink" OFF)

if(WIN32)
    option(LOGGER_HAVE_PTHREADS "Thread saftey" OFF)
endif()

message(STATUS "Have newline  : ${LOGGER_HAVE_NEWLINE}")
message(STATUS "Serial logsink: ${LOGGER_HAVE_SERIAL}")
if (NOT WIN32) 
    message(STATUS "Thread Saftey : ${LOGGER_HAVE_PTHREADS}")
endif()

project(logger)

set(CMAKE_SOURCE_DIR ./src)
set(CMAKE_BINARY_DIR ./bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ./lib)


set(CMAKE_BUILD_TYPE Debug)

include_directories("${PROJECT_SOURCE_DIR}")

#
# Logger Library
#
list(APPEND src_logger src/logger.cpp)
add_library(logger STATIC ${src_logger})
target_include_directories(logger PUBLIC ${CMAKE_SOURCE_DIR})

if(LOGGER_HAVE_NEWLINE) 
target_compile_definitions(logger PUBLIC LOGGER_HAVE_NEWLINE)
endif()
if(LOGGER_HAVE_PTHREADS) 
target_compile_definitions(logger PUBLIC LOGGER_HAVE_PTHREADS)
endif()
if(LOGGER_HAVE_SERIAL) 
target_compile_definitions(logger PUBLIC LOGGER_HAVE_SERIAL)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building debug mode....")
    target_compile_definitions(logger PUBLIC DEBUG)
endif()

set_property(TARGET logger PROPERTY CXX_STANDARD 11)




set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib)
# find_library(COCOA_FRAMEWORK Cocoa)
# find_library(IOKIT_FRAMEWORK IOKit)
# find_library(CORE_FRAMEWORK CoreFoundation)

# message(STATUS ${COCOA_FRAMEWORK})
# message(STATUS ${IOKIT_FRAMEWORK})
# message(STATUS ${CORE_FRAMEWORK})
# message(STATUS ${CMAKE_OSX_ARCHITECTURES})


add_executable(logtest main.cpp)
set_property(TARGET logtest PROPERTY CXX_STANDARD 11)
target_include_directories(logtest PUBLIC ./src)
target_link_libraries(logtest logger ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${CORE_FRAMEWORK})



