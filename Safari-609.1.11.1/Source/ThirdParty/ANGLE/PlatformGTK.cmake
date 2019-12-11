# Enable GLSL compiler output.
list(APPEND ANGLE_DEFINITIONS ANGLE_ENABLE_GLSL)

# NOTE: When both Wayland and X11 are enabled, ANGLE_USE_X11 will be
# defined and the X11 type definitions will be used for code involving
# both. That works because types for both Wayland and X11 have the same
# sizes and the code in WebKit casts the values to the proper types as
# needed.

if (ENABLE_WAYLAND_TARGET AND NOT ENABLE_X11_TARGET)
    list(APPEND ANGLE_DEFINITIONS WL_EGL_PLATFORM)
elseif (NOT ENABLE_X11_TARGET)
    # Allow building ANGLE on platforms which may not provide X11 headers.
    list(APPEND ANGLE_DEFINITIONS USE_SYSTEM_EGL)
endif ()

if (USE_ANGLE_EGL)
    list(APPEND ANGLE_SOURCES
        ${angle_system_utils_sources_linux}
        ${angle_system_utils_sources_posix}
        ${libangle_gl_sources}
        ${libangle_gl_glx_sources}
    )

    list(APPEND ANGLE_DEFINITIONS
        ANGLE_ENABLE_OPENGL
        ANGLE_USE_X11
    )

    list(APPEND ANGLEGLESv2_LIBRARIES
        dl
        pthread
        X11
    )

    string(REPLACE src/libGLESv2/libGLESv2_autogen.cpp "" libglesv2_sources "${libglesv2_sources}")
    string(REPLACE src/libEGL/libEGL.cpp "" libegl_sources "${libegl_sources}")
endif ()

