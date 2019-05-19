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
    include/pitaya/request.h
    src/pitaya.cpp
    # Protocol
    include/pitaya/protocol/packet.h
    src/protocol/packet.cpp
    include/pitaya/protocol/message.h
    src/protocol/message.cpp
    # Connection
    include/pitaya/connection.h
    include/pitaya/connection/event.h
    include/pitaya/connection/state.h
    src/connection.cpp
    src/utils/net.h
    src/utils/gzip.h
    src/utils/gzip.cpp
    src/connection/state.cpp
    # Connection
    include/pitaya/connection/packet_stream.h
    src/connection/tcp_packet_stream.h
    src/connection/tcp_packet_stream.cpp
    include/pitaya/connection/error.h
    src/connection/error.cpp
    # Logging
    src/logger.h
    # Socket
    include/pitaya/tcp_socket.h
    src/tcp_socket.cpp)

if(MSVC)
    target_compile_options(pitaya_cpp PRIVATE /WX /MD)
    target_compile_definitions(pitaya_cpp
      PRIVATE
        _CRT_SECURE_NO_WARNINGS
	-D_ITERATOR_DEBUG_LEVEL=0)
else()
    target_compile_options(pitaya_cpp PRIVATE -Wall -Werror)
endif()

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

if(MSVC)
    target_compile_options(example PRIVATE /MD)
    target_compile_definitions(example
      PRIVATE
        _CRT_SECURE_NO_WARNINGS
	-D_ITERATOR_DEBUG_LEVEL=0)
endif()

target_link_libraries(example PRIVATE pitaya_cpp)

#=========================================
# Tests
#=========================================
include(CTest)
option(BUILD_TESTING "" ON)
if(BUILD_TESTING AND NOT IOS AND NOT ANDROID)
    add_executable(pitaya_tests
        test/main.cpp
        test/tcp_packet_stream_test.cpp)

    set_target_properties(pitaya_tests
      PROPERTIES
        CXX_STANDARD 14
        OUTPUT_NAME tests)

    target_include_directories(pitaya_tests PUBLIC src include) 
    target_link_libraries(pitaya_tests PRIVATE CONAN_PKG::gtest pitaya_cpp)
endif()
