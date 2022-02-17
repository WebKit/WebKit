list(APPEND ANGLE_DEFINITIONS ANGLE_PLATFORM_LINUX)

if (USE_OPENGL)
    # Enable GLSL compiler output.
    list(APPEND ANGLE_DEFINITIONS ANGLE_ENABLE_GLSL)
endif ()

# NOTE: When both Wayland and X11 are enabled, ANGLE_USE_X11 will be
# defined and the X11 type definitions will be used for code involving
# both. That works because types for both Wayland and X11 have the same
# sizes and the code in WebKit casts the values to the proper types as
# needed.
if (ENABLE_X11_TARGET)
    list(APPEND ANGLE_DEFINITIONS ANGLE_USE_X11)
elseif (ENABLE_WAYLAND_TARGET)
    list(APPEND ANGLE_DEFINITIONS WL_EGL_PLATFORM)
else ()
    # Allow building ANGLE on platforms which may not provide X11 headers.
    list(APPEND ANGLE_DEFINITIONS USE_SYSTEM_EGL)
endif ()

if (USE_ANGLE_EGL OR USE_ANGLE_WEBGL)
    list(APPEND ANGLE_SOURCES
        ${_gl_backend_sources}

        ${angle_system_utils_sources_linux}
        ${angle_system_utils_sources_posix}

        ${libangle_gl_egl_dl_sources}
        ${libangle_gl_egl_sources}
        ${libangle_gl_sources}

        ${libangle_gpu_info_util_sources}
        ${libangle_gpu_info_util_linux_sources}
    )

    list(APPEND ANGLE_DEFINITIONS
        ANGLE_ENABLE_OPENGL
    )

    list(APPEND ANGLEGLESv2_LIBRARIES
        ${CMAKE_DL_LIBS}
        Threads::Threads
    )

    if (USE_OPENGL)
        list(APPEND ANGLEGLESv2_LIBRARIES OpenGL::OpenGL)
    else ()
        list(APPEND ANGLEGLESv2_LIBRARIES OpenGL::GLES)
    endif ()

    if (ENABLE_X11_TARGET)
        list(APPEND ANGLE_SOURCES ${libangle_gl_glx_sources})
        list(APPEND ANGLEGLESv2_LIBRARIES X11)
    endif ()
endif ()
