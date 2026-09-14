cmake_minimum_required(VERSION 3.20)

project(waystone_ring_buffer LANGUAGES CXX)

add_library(waystone_ring_buffer INTERFACE)
add_library(waystone::ring_buffer ALIAS waystone_ring_buffer)

target_compile_features(waystone_ring_buffer INTERFACE cxx_std_20)
target_include_directories(
    waystone_ring_buffer
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(PROJECT_IS_TOP_LEVEL)
    add_executable(ring_buffer_example examples/basic.cpp)
    target_link_libraries(ring_buffer_example PRIVATE waystone::ring_buffer)

    include(CTest)
    if(BUILD_TESTING)
        add_executable(ring_buffer_test tests/ring_buffer_test.cpp)
        target_link_libraries(ring_buffer_test PRIVATE waystone::ring_buffer)
        add_test(NAME ring_buffer_test COMMAND ring_buffer_test)
    endif()
endif()
