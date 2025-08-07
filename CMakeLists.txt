cmake_minimum_required(VERSION 3.16)
project(usercanal-cpp-sdk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# FlatBuffers
find_package(Flatbuffers REQUIRED)
if(NOT Flatbuffers_FOUND)
    message(FATAL_ERROR "FlatBuffers not found. Please install flatbuffers development package.")
endif()

# JSON library (nlohmann/json for properties handling) - DISABLED for now
# find_package(nlohmann_json QUIET)
# if(NOT nlohmann_json_FOUND)
#     message(STATUS "nlohmann_json not found via find_package, trying PkgConfig")
#     pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
# endif()

# Generate FlatBuffers headers
set(FLATBUFFERS_FLATC_EXECUTABLE flatc)
set(SCHEMA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/schema)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

file(MAKE_DIRECTORY ${GENERATED_DIR})

# Generate C++ headers from .fbs files
set(SCHEMA_FILES
    ${SCHEMA_DIR}/common.fbs
    ${SCHEMA_DIR}/event.fbs
    ${SCHEMA_DIR}/log.fbs
)

set(GENERATED_HEADERS)
foreach(SCHEMA_FILE ${SCHEMA_FILES})
    get_filename_component(SCHEMA_NAME ${SCHEMA_FILE} NAME_WE)
    set(GENERATED_HEADER ${GENERATED_DIR}/${SCHEMA_NAME}_generated.h)
    list(APPEND GENERATED_HEADERS ${GENERATED_HEADER})

    add_custom_command(
        OUTPUT ${GENERATED_HEADER}
        COMMAND ${FLATBUFFERS_FLATC_EXECUTABLE} --cpp -o ${GENERATED_DIR} ${SCHEMA_FILE}
        DEPENDS ${SCHEMA_FILE}
        COMMENT "Generating FlatBuffers header for ${SCHEMA_NAME}"
    )
endforeach()

add_custom_target(generate_flatbuffers DEPENDS ${GENERATED_HEADERS})

# Source files
set(SOURCES
    src/client.cpp
    src/config.cpp
    src/types.cpp
    src/network.cpp
    src/batch.cpp
    src/errors.cpp
    src/utils.cpp
    src/hooks.cpp
    src/observability.cpp
    src/pipeline.cpp
    src/session.cpp
)

set(HEADERS
    include/usercanal/client.hpp
    include/usercanal/config.hpp
    include/usercanal/types.hpp
    include/usercanal/network.hpp
    include/usercanal/batch.hpp
    include/usercanal/errors.hpp
    include/usercanal/utils.hpp
    include/usercanal/usercanal.hpp
    include/usercanal/hooks.hpp
    include/usercanal/observability.hpp
    include/usercanal/pipeline.hpp
    include/usercanal/session.hpp
)

# Create the main library
add_library(usercanal ${SOURCES} ${HEADERS})
add_dependencies(usercanal generate_flatbuffers)

target_include_directories(usercanal
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${GENERATED_DIR}>
    PRIVATE
        src
)

target_link_libraries(usercanal
    PUBLIC
        Threads::Threads
        flatbuffers::flatbuffers
    # PRIVATE
    #     $<IF:$<TARGET_EXISTS:nlohmann_json::nlohmann_json>,nlohmann_json::nlohmann_json,${NLOHMANN_JSON_LIBRARIES}>
)

# if(nlohmann_json_FOUND)
#     target_compile_definitions(usercanal PRIVATE NLOHMANN_JSON_FOUND)
# else()
#     target_include_directories(usercanal PRIVATE ${NLOHMANN_JSON_INCLUDE_DIRS})
#     target_compile_options(usercanal PRIVATE ${NLOHMANN_JSON_CFLAGS_OTHER})
# endif()

# Compiler-specific options
target_compile_options(usercanal PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic -O3>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic -O3>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /O2>
)

# Debug build options
target_compile_definitions(usercanal PRIVATE
    $<$<CONFIG:Debug>:USERCANAL_DEBUG>
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest QUIET)
    if(GTest_FOUND)
        add_subdirectory(tests)
    else()
        message(WARNING "Google Test not found. Tests will not be built.")
    endif()
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS usercanal
    EXPORT usercanal-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${GENERATED_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/usercanal/generated)

install(EXPORT usercanal-targets
    FILE usercanal-targets.cmake
    NAMESPACE usercanal::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/usercanal
)

# Generate config files
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/usercanal-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/usercanal-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/usercanal
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/usercanal-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/usercanal-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/usercanal-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/usercanal
)

# Package configuration
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_NAME "usercanal-cpp-sdk")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "UserCanal C++ SDK for high-performance analytics and logging")
include(CPack)
