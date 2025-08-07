cmake_minimum_required(VERSION 3.16)

# User-focused example sources
set(EXAMPLE_SOURCES
    basic_foundation.cpp
    event_simple.cpp
    log_simple.cpp
    log_severities.cpp
    event_revenue.cpp
)

# Create example executables
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)

    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})

    target_link_libraries(${EXAMPLE_NAME}
        PRIVATE
            usercanal
            Threads::Threads
    )

    target_include_directories(${EXAMPLE_NAME}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_BINARY_DIR}/../generated
    )

    # Add dependency on FlatBuffers generation
    add_dependencies(${EXAMPLE_NAME} generate_flatbuffers)

    # Compiler options
    target_compile_options(${EXAMPLE_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )

    # Install example
    install(TARGETS ${EXAMPLE_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    )
endforeach()

# Custom targets for running examples
add_custom_target(run_basic_foundation
    COMMAND basic_foundation
    DEPENDS basic_foundation
    COMMENT "Running getting started example"
)

add_custom_target(run_event_simple
    COMMAND event_simple
    DEPENDS event_simple
    COMMENT "Running simple event tracking example"
)

add_custom_target(run_log_simple
    COMMAND log_simple
    DEPENDS log_simple
    COMMENT "Running simple logging example"
)

add_custom_target(run_log_severities
    COMMAND log_severities
    DEPENDS log_severities
    COMMENT "Running log severity levels example"
)

add_custom_target(run_event_revenue
    COMMAND event_revenue
    DEPENDS event_revenue
    COMMENT "Running revenue tracking example"
)
