# ---------------------------------------------------------------------------------------
# This insanity is for people embedding libavif or making fully static or Windows builds.
# Any proper unix environment should ignore these entire following blocks.
# Put the value of ZLIB_INCLUDE_DIR in the cache. This works around cmake behavior that has been updated by
# cmake policy CMP0102 in cmake 3.17. Remove the CACHE workaround when we require cmake 3.17 or later. See
# https://gitlab.kitware.com/cmake/cmake/-/issues/21343.
set(ZLIB_INCLUDE_DIR "${AVIF_SOURCE_DIR}/ext/zlib" CACHE PATH "zlib include dir")
# This include_directories() call must be before add_subdirectory(ext/zlib) to work around the
# zlib/CMakeLists.txt bug fixed by https://github.com/madler/zlib/pull/818.
include_directories(SYSTEM $<BUILD_INTERFACE:${ZLIB_INCLUDE_DIR}>)

add_subdirectory(ext/zlib EXCLUDE_FROM_ALL)

# Re-enable example and example64 targets, as these are used by tests
if(AVIF_BUILD_TESTS)
    set_property(TARGET example PROPERTY EXCLUDE_FROM_ALL FALSE)
    if(TARGET example64)
        set_property(TARGET example64 PROPERTY EXCLUDE_FROM_ALL FALSE)
    endif()
endif()

target_include_directories(zlibstatic INTERFACE $<BUILD_INTERFACE:${ZLIB_INCLUDE_DIR}>)

# This include_directories() call and the previous include_directories() call provide the zlib
# include directories for add_subdirectory(ext/libpng). Because we set PNG_BUILD_ZLIB,
# libpng/CMakeLists.txt won't call find_package(ZLIB REQUIRED) and will see an empty
# ${ZLIB_INCLUDE_DIRS}.
include_directories($<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/ext/zlib>)
set(CMAKE_DEBUG_POSTFIX "")

add_library(ZLIB::ZLIB ALIAS zlibstatic)

# This is the only way I could avoid libpng going crazy if it found awk.exe, seems benign otherwise
set(PREV_ANDROID ${ANDROID})
set(ANDROID TRUE)
set(PNG_BUILD_ZLIB "${AVIF_SOURCE_DIR}/ext/zlib" CACHE STRING "" FORCE)
set(PNG_SHARED OFF CACHE BOOL "")
set(PNG_TESTS OFF CACHE BOOL "")
set(PNG_EXECUTABLES OFF CACHE BOOL "")

add_subdirectory("${AVIF_SOURCE_DIR}/ext/libpng" EXCLUDE_FROM_ALL)

set(PNG_PNG_INCLUDE_DIR "${AVIF_SOURCE_DIR}/ext/libpng")
include_directories($<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/ext/libpng>)
set(ANDROID ${PREV_ANDROID})

set_target_properties(png_static zlibstatic PROPERTIES AVIF_LOCAL ON)
add_library(PNG::PNG ALIAS png_static)
