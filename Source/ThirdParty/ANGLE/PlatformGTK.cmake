list(APPEND ANGLE_DEFINITIONS ANGLE_PLATFORM_LINUX)
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
        ANGLE_USE_GBM
        USE_SYSTEM_EGL
    )

    find_package(LibDRM REQUIRED)
    find_package(GBM REQUIRED)

    list(APPEND ANGLE_PRIVATE_INCLUDE_DIRECTORIES
        ${LIBDRM_INCLUDE_DIR}
        {GBM_INCLUDE_DIR}
    )

    list(APPEND ANGLEGLESv2_LIBRARIES
        ${CMAKE_DL_LIBS}
        ${LIBDRM_LIBRARIES}
        ${GBM_LIBRARIES}
        Threads::Threads
    )

    if (USE_OPENGL)
        list(APPEND ANGLEGLESv2_LIBRARIES ${OPENGL_LIBRARIES})
    else ()
        list(APPEND ANGLEGLESv2_LIBRARIES OpenGL::GLES)
    endif ()
endif ()
