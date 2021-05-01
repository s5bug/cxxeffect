cmake_minimum_required(VERSION 3.10)

project(cxxeffect)

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

