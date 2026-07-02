include(PlatformCocoa.cmake)

find_library(IOKIT_LIBRARY IOKit)
find_library(QUARTZ_LIBRARY Quartz)

list(APPEND ANGLE_SOURCES
    ${libangle_gpu_info_util_mac_sources}
)

list(APPEND ANGLEGLESv2_LIBRARIES
    ${IOKIT_LIBRARY}
    ${QUARTZ_LIBRARY}
)
