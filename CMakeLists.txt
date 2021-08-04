cmake_minimum_required(VERSION 3.10)

project(cxxeffect)

# We want to export compile commands b/c it's useful for ccls
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

# add the executable
add_executable(cxxeffect
    src/stream.hpp
    src/stream.cpp
    src/task.hpp
    src/task.cpp
    src/eff.hpp
    src/eff.cpp)

set_property(TARGET cxxeffect PROPERTY CXX_STANDARD 17)

target_link_libraries(cxxeffect pthread)

