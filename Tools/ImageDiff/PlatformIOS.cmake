find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(COREGRAPHICS_LIBRARY CoreGraphics)
find_library(CORETEXT_LIBRARY CoreText)
find_library(IMAGEIO_LIBRARY ImageIO)

list(APPEND ImageDiff_SOURCES
    cg/PlatformImageCG.cpp
)

list(APPEND ImageDiff_LIBRARIES
    ${COREFOUNDATION_LIBRARY}
    ${COREGRAPHICS_LIBRARY}
    ${CORETEXT_LIBRARY}
    ${IMAGEIO_LIBRARY}
)
