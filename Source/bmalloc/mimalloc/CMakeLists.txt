set(MI_OVERRIDE OFF CACHE BOOL "Disable mimalloc override" FORCE)
set(MI_OSX_INTERPOSE OFF CACHE BOOL "Disable macOS malloc zone interpose" FORCE)
set(MI_OSX_ZONE ON CACHE BOOL "Disable macOS malloc zone" FORCE)
set(MI_BUILD_SHARED OFF CACHE BOOL "Build mimalloc shared library" FORCE)
set(MI_BUILD_STATIC OFF CACHE BOOL "Build mimalloc static library" FORCE)
set(MI_BUILD_OBJECT ON CACHE BOOL "Build mimalloc object library" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "Build mimalloc tests" FORCE)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

add_subdirectory(mimalloc)
WEBKIT_ADD_TARGET_CXX_FLAGS(mimalloc-obj
    -Wno-undef
    -Wno-cast-align)
