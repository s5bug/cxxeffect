cmake_minimum_required(VERSION 3.10)

project(cxxeffect CXX)

# Register dependencies
add_subdirectory(modules/conduit)

# We want to export compile commands b/c it's useful for ccls
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

# cxxeffect is an interface library b/c it has a lot of templated code
# That means it's best implemented as header-only without the need for cpp files
add_library(cxxeffect INTERFACE)

set_property(TARGET cxxeffect PROPERTY CXX_STANDARD_REQUIRED 20)

target_compile_features(cxxeffect INTERFACE cxx_std_20)

target_link_libraries(cxxeffect INTERFACE conduit)

# The /EHa flag enables standard C++ stack unwinding
# See: https://docs.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=msvc-160
if (MSVC)
    target_compile_options(cxxeffect INTERFACE "/EHa")
endif()

# Enables GCC support for coroutines (these are standard C++ now but GCC still
# requires a flag for them)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    target_compile_options(cxxeffect INTERFACE "-fcoroutines")
endif()

target_compile_definitions(cxxeffect INTERFACE)

target_include_directories(
    cxxeffect
    INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:include>
)

# Provides install target, so users can install the library on their system
# if they like

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION include
)

# Use pthreads, if availible
set(THREADS_PREFER_PTHREAD_FLAG ON)
# Enable threads in project
find_package(Threads REQUIRED)

# If we're building this project, and not something that has this project as a dependency
# Then we want to build files in the examples directory (which contains the cat)
# This will compile every file in the examples directory as a separate program
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    file(GLOB all_examples "examples/*.cpp")

    foreach(file_name ${all_examples})
        get_filename_component(executable_name ${file_name} NAME_WE)
        add_executable("${executable_name}" "${file_name}")
        target_link_libraries("${executable_name}" PRIVATE cxxeffect)
        target_link_libraries("${executable_name}" PRIVATE Threads::Threads)
    endforeach()
endif()


# Boilerplate code to compile unit tests inside a test/ directory

# if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
#     include(CTest)
# endif()
#
# if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
#     set(THREADS_PREFER_PTHREAD_FLAG ON)
#     find_package(Threads REQUIRED)
#
#     file(GLOB all_tests "test/*.cpp")
#
#     foreach(file_name ${all_tests})
#         get_filename_component(test_name ${file_name} NAME_WE)
#         add_executable("${test_name}" "${file_name}")
#         target_link_libraries("${test_name}" PRIVATE cxxeffect)
#         target_link_libraries("${test_name}" PRIVATE Threads::Threads)
#         add_test(NAME "${test_name}" COMMAND "${test_name}")
#     endforeach()
# endif()
