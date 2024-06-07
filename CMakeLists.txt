cmake_minimum_required(VERSION 3.10)
project(OS_ex2 C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Find the GLib package
#find_package(PkgConfig REQUIRED)
#pkg_check_modules(GLIB REQUIRED glib-2.0)

#include_directories(${GLIB_INCLUDE_DIRS})
#link_directories(${GLIB_LIBRARY_DIRS})

add_executable(OS_ex2
        uthreads.cpp
        ex2_test.cpp
        uthreads.h)

# Link against GLib
#target_link_libraries(OS_ex2 ${GLIB_LIBRARIES})