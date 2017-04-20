list(APPEND ANGLE_SOURCES
    src/common/system_utils_mac.cpp
    src/libANGLE/renderer/gl/cgl/DisplayCGL.mm
    src/libANGLE/renderer/gl/cgl/PbufferSurfaceCGL.mm
    src/libANGLE/renderer/gl/cgl/WindowSurfaceCGL.mm
)

find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(OPENGL_LIBRARY OpenGL)
find_library(QUARTZCORE_LIBRARY QuartzCore)
list(APPEND ANGLEGLESv2_LIBRARIES
    PRIVATE ${COREGRAPHICS_LIBRARY}
    PRIVATE ${OPENGL_LIBRARY}
    PRIVATE ${QUARTZCORE_LIBRARY}
)
