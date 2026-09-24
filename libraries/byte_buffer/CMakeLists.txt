cmake_minimum_required(VERSION 3.20)

project(waystone_byte_buffer LANGUAGES CXX)

add_library(waystone_byte_buffer INTERFACE)
add_library(waystone::byte_buffer ALIAS waystone_byte_buffer)

target_compile_features(waystone_byte_buffer INTERFACE cxx_std_20)
target_include_directories(
    waystone_byte_buffer
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(PROJECT_IS_TOP_LEVEL)
    add_executable(byte_buffer_example examples/basic.cpp)
    target_link_libraries(byte_buffer_example PRIVATE waystone::byte_buffer)

    include(CTest)
    if(BUILD_TESTING)
        add_executable(byte_buffer_test tests/byte_buffer_test.cpp)
        target_link_libraries(byte_buffer_test PRIVATE waystone::byte_buffer)
        add_test(NAME byte_buffer_test COMMAND byte_buffer_test)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            byte_buffer_example PRIVATE -Wall -Wextra -Wpedantic -Werror
        )
        if(BUILD_TESTING)
            target_compile_options(
                byte_buffer_test PRIVATE -Wall -Wextra -Wpedantic -Werror
            )
        endif()
    endif()
endif()
