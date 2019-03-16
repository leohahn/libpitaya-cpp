cmake_minimum_required(VERSION 3.7)
project(LibPitayaCpp)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
endif()

#=========================================
# Pitaya target
#=========================================
find_package(Threads REQUIRED)

add_library(pitaya_cpp
    include/pitaya.h
    include/pitaya/exception.h
    include/pitaya/connection/event.h
    src/pitaya.cpp
    # Protocol
    include/pitaya/protocol/packet.h
    src/protocol/packet.cpp
    # Connection
    include/pitaya/connection.h
    src/connection.cpp
    src/utils/net.h
    src/utils/gzip.h
    src/utils/gzip.cpp
    include/pitaya/connection/state.h
    src/connection/state.cpp
    # Connection
    include/pitaya/connection/packet_stream.h
    src/connection/tcp_packet_stream.h
    src/connection/tcp_packet_stream.cpp
    include/pitaya/connection/error.h
    src/connection/error.cpp
    # Logging
    src/logger.h
    src/logger.cpp
    )

target_compile_options(pitaya_cpp PRIVATE -Wall)

target_include_directories(pitaya_cpp
    PUBLIC include
    PRIVATE src)

set_target_properties(pitaya_cpp
    PROPERTIES CXX_STANDARD 14)

target_link_libraries(pitaya_cpp
    PUBLIC
        CONAN_PKG::boost_asio
        Threads::Threads
    PRIVATE
        CONAN_PKG::rapidjson
        CONAN_PKG::zlib)

#=========================================
# Example target
#=========================================
add_executable(example
    src/example.cpp)

set_target_properties(example
    PROPERTIES CXX_STANDARD 14)

target_link_libraries(example PRIVATE pitaya_cpp)
