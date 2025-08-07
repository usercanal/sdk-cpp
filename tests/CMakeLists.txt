cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest REQUIRED)

# Test sources
set(TEST_SOURCES
    test_foundation.cpp
    test_phase2.cpp
)

# Create test executable
add_executable(usercanal_tests ${TEST_SOURCES})

# Link against the library and Google Test
target_link_libraries(usercanal_tests
    PRIVATE
        usercanal
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
)

# Include directories
target_include_directories(usercanal_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_BINARY_DIR}/../generated
)

# Add dependency on FlatBuffers generation
add_dependencies(usercanal_tests generate_flatbuffers)

# Compiler options
target_compile_options(usercanal_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(usercanal_tests)

# Custom test targets
add_custom_target(test_foundation
    COMMAND usercanal_tests --gtest_filter="FoundationTest*"
    DEPENDS usercanal_tests
    COMMENT "Running foundation tests"
)

add_custom_target(test_phase2
    COMMAND usercanal_tests --gtest_filter="Phase2Test*"
    DEPENDS usercanal_tests
    COMMENT "Running Phase 2 tests"
)

add_custom_target(test_performance
    COMMAND usercanal_tests --gtest_filter="*Performance*"
    DEPENDS usercanal_tests
    COMMENT "Running performance tests"
)

add_custom_target(test_all
    COMMAND usercanal_tests
    DEPENDS usercanal_tests
    COMMENT "Running all tests"
)
