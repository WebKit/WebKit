# We're targeting Windows 10 which will have DirectX 11 on it so require that
# but make DirectX 9 optional

list(APPEND ANGLE_DEFINITIONS
    GL_APICALL=
    GL_API=
    NOMINMAX
)

if (USE_ANGLE_EGL)
    # We're targeting Windows 10 which will have DirectX 11
    list(APPEND ANGLE_SOURCES
        ${angle_system_utils_sources_win}
        ${angle_translator_hlsl_sources}

        ${libangle_d3d_shared_sources}
        ${libangle_d3d11_sources}
        ${libangle_d3d11_win32_sources}
    )

    list(APPEND ANGLE_DEFINITIONS
        ANGLE_ENABLE_HLSL
        ANGLE_ENABLE_D3D11
    )

    list(APPEND ANGLEGLESv2_LIBRARIES dxguid)

    # DirectX 9 support should be optional but ANGLE will not compile without it
    list(APPEND ANGLE_SOURCES ${libangle_d3d9_sources})
    list(APPEND ANGLE_DEFINITIONS ANGLE_ENABLE_D3D9)
    list(APPEND ANGLEGLESv2_LIBRARIES d3d9)
endif ()
