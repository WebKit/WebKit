find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(FOUNDATION_LIBRARY Foundation)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(METAL_LIBRARY Metal)
if (NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB REQUIRED)
endif ()

list(APPEND ANGLE_SOURCES
    ${metal_backend_sources}

    ${angle_translator_lib_msl_sources}

    ${libangle_mac_sources}
    ${libangle_gpu_info_util_sources}
)

list(APPEND ANGLE_DEFINITIONS
    ANGLE_ENABLE_METAL
)

list(APPEND ANGLEGLESv2_LIBRARIES
    ${COREGRAPHICS_LIBRARY}
    ${FOUNDATION_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${METAL_LIBRARY}
)
