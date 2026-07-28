if (WEBKIT_SDK_IS_MACOS)

find_package(Apple REQUIRED COMPONENTS CoreFoundation CoreGraphics CoreServices CoreText ImageIO)

include(CoreGraphics.cmake)

# CoreServices provides kUTTypePNG on macOS (guarded by __APPLE__ in PlatformImageCG.cpp).
list(APPEND ImageDiff_LIBRARIES
    Apple::CoreServices
)

# kUTTypePNG deprecated in 12.0; our deployment target is 26.2.
list(APPEND ImageDiff_COMPILE_OPTIONS -Wno-deprecated-declarations)

# FIXME: Remove once source files are fixed. https://bugs.webkit.org/show_bug.cgi?id=312034
list(APPEND ImageDiff_COMPILE_OPTIONS -Wno-unused-parameter)

elseif (WEBKIT_SDK_IS_IOS_FAMILY)

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

endif ()
