set_property(DIRECTORY . PROPERTY FOLDER "sysprof-capture")

add_library(SysProfCapture STATIC
    mapped-ring-buffer.c
    sysprof-address.c
    sysprof-capture-condition.c
    sysprof-capture-cursor.c
    sysprof-capture-reader.c
    sysprof-capture-util.c
    sysprof-capture-writer.c
    sysprof-capture-writer-cat.c
    sysprof-collector.c
    sysprof-clock.c
    sysprof-platform.c
)

target_compile_definitions(SysProfCapture PRIVATE
    SYSPROF_CAPTURE_COMPILATION
)

# We want te dummy config.h file used only for libsysprof-capture,
# having it in a separate directory we can mark that one as PRIVATE.
target_include_directories(SysProfCapture PRIVATE include)

WEBKIT_CHECK_HAVE_FUNCTION(HAVE_REALLOCARRAY reallocarray stdlib.h)
if (HAVE_REALLOCARRAY)
    target_compile_definitions(SysProfCapture PRIVATE HAVE_REALLOCARRAY)
endif ()

WEBKIT_CHECK_HAVE_FUNCTION(HAVE_STRLCPY strlcpy string.h)
if (HAVE_STRLCPY)
    target_compile_definitions(SysProfCapture PRIVATE HAVE_STRLCPY)
endif ()

set_target_properties(SysProfCapture PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE TRUE
    VISIBILITY_INLINES_HIDDEN TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(SysProfCapture
    PRIVATE
    Threads::Threads
)

WEBKIT_ADD_TARGET_C_FLAGS(SysProfCapture
    -Wno-implicit-function-declaration
    -Wno-sign-compare
    -Wno-unused-parameter
)

add_library(SysProfCapture::SysProfCapture ALIAS SysProfCapture)
