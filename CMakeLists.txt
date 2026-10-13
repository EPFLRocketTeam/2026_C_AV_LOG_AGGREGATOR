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

#############################################################
######################## UNIT TESTS ##########################
#############################################################
# Only when this repo is the root project. Firmware builds that add it as a
# subdirectory get the library target and nothing else.

if (PROJECT_IS_TOP_LEVEL)
    option(ENABLE_COVERAGE "Enable code coverage instrumentation (requires gcov/lcov)" OFF)
    find_program(VALGRIND_EXECUTABLE NAMES valgrind)

    if (ENABLE_COVERAGE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage -O0")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -O0")

        set(MEMORYCHECK_COMMAND "${VALGRIND_EXECUTABLE}")
        set(MEMORYCHECK_COMMAND_OPTIONS "--tool=memcheck --leak-check=full --track-fds=yes --error-exitcode=1")

        add_custom_target(
            memcheck

            COMMAND ctest -T memcheck
        )
        add_custom_target(
            coverage

            COMMAND lcov --directory . --zerocounters --rc branch_coverage=1
            COMMAND ctest

            COMMAND lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch --rc branch_coverage=1
            COMMAND lcov --remove coverage.info '/usr/*' '*test*' --output-file coverage_filtered.info --rc branch_coverage=1

            COMMAND genhtml coverage_filtered.info --output-directory html_report --rc branch_coverage=1
        )
    endif()

    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    include(CTest)
    set(CTEST_TIMEOUT 30)

    include(GoogleTest)

    function(create_test test_name test_file)
        add_executable(${test_name} ${test_file})
        target_link_libraries(${test_name} PRIVATE GTest::gtest_main log_aggregator)
        gtest_discover_tests(${test_name})
    endfunction()

    create_test(test_chunker     tests/test_chunker.cpp)
    create_test(test_reassembler tests/test_reassembler.cpp)
endif()
