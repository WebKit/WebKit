# Enable ccache by default for the Mac port, if installed.
# Setting WK_USE_CCACHE=NO in your environment will disable it.
if (PORT STREQUAL "Mac" AND NOT "$ENV{WK_USE_CCACHE}" STREQUAL "NO")
    find_program(CCACHE_FOUND ccache)
    if (CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CMAKE_SOURCE_DIR}/Tools/ccache/ccache-wrapper)
    endif ()
endif ()
