# Set the minimum required version of CMake for this project.
cmake_minimum_required(VERSION 2.8)

# Setup some options.
option(BUILD_SHARED "Builds the library as shared library" OFF)
option(USE_LIBCXX "Uses libc++ instead of libstdc++" ON)
option(USE_CUSTOM_LIBCXX "Uses a custom libc++" OFF)

add_definitions( -DVR_API_PUBLIC )

# Check if 32 or 64 bit system.
set(SIZEOF_VOIDP ${CMAKE_SIZEOF_VOID_P})
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PROCESSOR_ARCH "64")
else()
  set(PROCESSOR_ARCH "32")
endif()

# Get platform.
if(WIN32)
       set(PLATFORM_NAME "win")
elseif(UNIX AND NOT APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    set(PLATFORM_NAME "linux")
    add_definitions(-DLINUX -DPOSIX)
    if(PROCESSOR_ARCH MATCHES "64")
        add_definitions(-DLINUX64)
    endif()
  endif()
elseif(APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*" OR CMAKE_SYSTEM_NAME MATCHES ".*MacOS.*")
    set(PLATFORM_NAME "osx")
    add_definitions(-DOSX -DPOSIX)
  endif()
endif()

# Enable some properties.
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
       # Enable c++11 and hide symbols which shouldn't be visible
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -fvisibility=hidden")

       # Set custom libc++ usage here
       if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND USE_LIBCXX)
              if(USE_CUSTOM_LIBCXX)
                     if(BUILD_SHARED)
                            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
                     endif()
                     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostdinc++")
                     include_directories( ${LIBCXX_INCLUDE} ${LIBCXX_ABI_INCLUDE})
                     message(STATUS "Using custom libc++")
              else()
                     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
                     message(STATUS "Using libc++")
              endif()
       endif()
endif()

add_subdirectory(src)
