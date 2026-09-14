cmake_minimum_required(VERSION 3.20)

project(object_lifetime_lab LANGUAGES CXX)

add_executable(object_lifetime_lab main.cpp)
target_compile_features(object_lifetime_lab PRIVATE cxx_std_20)

enable_testing()
add_test(NAME object_lifetime_lab COMMAND object_lifetime_lab)
