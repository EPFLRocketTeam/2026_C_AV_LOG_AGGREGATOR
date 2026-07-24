cmake_minimum_required(VERSION 3.28.0)
PROJECT(2026_C_AV_LOG_AGGREGATOR)
SET(CMAKE_C_FLAGS "-g -Wall -Werror")
SET(CMAKE_CXX_FLAGS "-g -Wall -Werror -std=c++17")

#############################################################
################## LOG AGGREGATOR CODEBASE ###################
#############################################################

# Add your source files here
SET(LOG_AGGREGATOR_SOURCE_FILES
    src/log_aggregator/chunker.cpp
    src/log_aggregator/reassembler.cpp)

add_library(log_aggregator STATIC ${LOG_AGGREGATOR_SOURCE_FILES})
target_include_directories(log_aggregator PUBLIC include)
