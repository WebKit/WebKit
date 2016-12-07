list(APPEND ANGLE_SOURCES
    src/libANGLE/renderer/gl/cgl/DisplayCGL.mm
    src/libANGLE/renderer/gl/cgl/PbufferSurfaceCGL.mm
    src/libANGLE/renderer/gl/cgl/WindowSurfaceCGL.mm
)

find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(OPENGL_LIBRARY OpenGL)
find_library(QUARTZCORE_LIBRARY QuartzCore)
list(APPEND ANGLEGLESv2_LIBRARIES
    ${COREGRAPHICS_LIBRARY}
    ${OPENGL_LIBRARY}
    ${QUARTZCORE_LIBRARY}
)
