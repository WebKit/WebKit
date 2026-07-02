include(PlatformCocoa.cmake)

find_library(QUARTZCORE_LIBRARY QuartzCore)

list(REMOVE_ITEM ANGLE_SOURCES
    src/common/gl/cgl/FunctionsCGL.cpp
    src/common/gl/cgl/FunctionsCGL.h
    src/common/system_utils_mac.cpp
)

list(APPEND ANGLE_SOURCES
    ${libangle_gpu_info_util_ios_sources}
    src/libANGLE/renderer/driver_utils_ios.mm
)

list(APPEND ANGLEGLESv2_LIBRARIES
    ${QUARTZCORE_LIBRARY}
)
