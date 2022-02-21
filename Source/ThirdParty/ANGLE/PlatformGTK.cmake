list(APPEND ANGLE_DEFINITIONS ANGLE_PLATFORM_LINUX USE_SYSTEM_EGL)
include(linux.cmake)

if (USE_OPENGL)
    # Enable GLSL compiler output.
    list(APPEND ANGLE_DEFINITIONS ANGLE_ENABLE_GLSL)
endif ()

if (USE_ANGLE_EGL OR USE_ANGLE_WEBGL)
    list(APPEND ANGLE_SOURCES
        ${_gl_backend_sources}

        ${angle_system_utils_sources_linux}
        ${angle_system_utils_sources_posix}

        ${angle_dma_buf_sources}

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
endif ()
