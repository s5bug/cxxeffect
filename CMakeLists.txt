cmake_minimum_required(VERSION 3.22)
project(cxxeffect)

set(CMAKE_CXX_STANDARD 20)

add_library(cxxeffect INTERFACE)
target_sources(cxxeffect INTERFACE
        cxxeffect/eff.hpp
        cxxeffect/eff/task.hpp)
target_include_directories(cxxeffect INTERFACE cxxeffect)

add_executable(test test/main.cpp)
target_link_libraries(test PRIVATE cxxeffect)

# Compiler options
if(MSVC)
    target_compile_options(test PRIVATE /W4 /WX)
else()
    target_compile_options(test PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Platform-specific defines
if(MSVC)
    target_compile_definitions(test PRIVATE
            -DUNICODE -D_UNICODE
            -D_WIN32_WINNT=0x0A00
            -DWIN32_LEAN_AND_MEAN)
endif()
