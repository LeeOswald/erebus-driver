cmake_minimum_required(VERSION 3.20)

function(get_version_from_file VER_FILE_NAME)
    file(READ ${VER_FILE_NAME} ERK_VERSION_RAW)
    # Remove trailing whitespaces and/or newline
    string(STRIP ${ERK_VERSION_RAW} ERK_VERSION_)
    set(ERK_VERSION ${ERK_VERSION_} CACHE STRING
        "Project version determined from version.txt" FORCE
    )
    message(STATUS "Determined project version ${ERK_VERSION}")
endfunction()


get_version_from_file("version.txt")

project(erebus-driver VERSION ${ERK_VERSION})

if(NOT CMAKE_BUILD_TYPE)
    message(WARNING "CMAKE_BUILD_TYPE isn't defined. Debug value will be used by default.")
    set(CMAKE_BUILD_TYPE Debug)
else()
    if(${CMAKE_BUILD_TYPE} MATCHES "^[Rr][Ee][Ll][Ee][Aa][Ss][Ee]$")
        set(CMAKE_BUILD_TYPE Release)
    elseif(${CMAKE_BUILD_TYPE} MATCHES "^[Dd][Ee][Bb][Uu][Gg]$")
        set(CMAKE_BUILD_TYPE Debug)
    else()
        message(WARNING "CMAKE_BUILD_TYPE isn't Release or Debug. Debug value will be used by default.")
        set(CMAKE_BUILD_TYPE Debug)
    endif()
endif()

# -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# -fvisibility=hidden
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_link_options(-rdynamic) # enable stack capture

set(ER_LINUX 0)
set(ER_WINDOWS 0)
set(ER_POSIX 0)

set(CMAKE_CXX_STANDARD 20)
set(EREBUS_CXX_FEATURES cxx_std_20)

if(UNIX AND NOT APPLE)
    set(ER_LINUX 1)
    set(ER_POSIX 1)
else()
    message(FATAL_ERROR "Unsupported OS")
endif()

# require Boost
find_package(Boost REQUIRED COMPONENTS 
    stacktrace_basic
    stacktrace_backtrace
    stacktrace_addr2line
    stacktrace_noop 
    system
    program_options
)

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/driver-version.h.in driver-version.h)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(${PROJECT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/submodules/erebus-server/include ${CMAKE_CURRENT_SOURCE_DIR}/include)


# dependencies
set(EREBUS_RTLLIB erebus-rtl)

# component names
set(EREBUS_DRIVERLIB erebus-kernel)
set(EREBUS_KMTEST erkm-test)

add_subdirectory(submodules/erebus-server)
add_subdirectory(erebus-kernel)
add_subdirectory(kmtest)
