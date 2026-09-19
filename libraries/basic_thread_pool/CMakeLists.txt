cmake_minimum_required(VERSION 3.20)

project(waystone_basic_thread_pool LANGUAGES CXX)

find_package(Threads REQUIRED)

add_library(waystone_basic_thread_pool INTERFACE)
add_library(waystone::basic_thread_pool ALIAS waystone_basic_thread_pool)

target_compile_features(waystone_basic_thread_pool INTERFACE cxx_std_20)
target_include_directories(
    waystone_basic_thread_pool
    INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(waystone_basic_thread_pool INTERFACE Threads::Threads)

if(PROJECT_IS_TOP_LEVEL)
    add_executable(basic_thread_pool_example examples/basic.cpp)
    target_link_libraries(basic_thread_pool_example PRIVATE waystone::basic_thread_pool)

    include(CTest)
    if(BUILD_TESTING)
        add_executable(basic_thread_pool_test tests/basic_thread_pool_test.cpp)
        target_link_libraries(basic_thread_pool_test PRIVATE waystone::basic_thread_pool)
        add_test(NAME basic_thread_pool_test COMMAND basic_thread_pool_test)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            basic_thread_pool_example PRIVATE -Wall -Wextra -Wpedantic -Werror
        )
        if(BUILD_TESTING)
            target_compile_options(
                basic_thread_pool_test PRIVATE -Wall -Wextra -Wpedantic -Werror
            )
        endif()
    endif()
endif()
