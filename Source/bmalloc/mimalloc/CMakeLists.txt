set(MI_OVERRIDE OFF CACHE BOOL "Disable mimalloc override" FORCE)
set(MI_OSX_INTERPOSE OFF CACHE BOOL "Disable macOS malloc zone interpose" FORCE)
set(MI_OSX_ZONE ON CACHE BOOL "Disable macOS malloc zone" FORCE)
set(MI_BUILD_SHARED OFF CACHE BOOL "Build mimalloc shared library" FORCE)
set(MI_BUILD_STATIC OFF CACHE BOOL "Build mimalloc static library" FORCE)
set(MI_BUILD_OBJECT ON CACHE BOOL "Build mimalloc object library" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "Build mimalloc tests" FORCE)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (CMAKE_VERSION VERSION_LESS "3.25")
    message(FATAL_ERROR "CMake 3.25 is required for USE_MIMALLOC, but current CMake version is ${CMAKE_VERSION}")
endif ()

add_subdirectory(mimalloc EXCLUDE_FROM_ALL)
WEBKIT_ADD_TARGET_CXX_FLAGS(mimalloc-obj
    -Wno-undef
    -Wno-cast-align)
