find_package(Apple REQUIRED COMPONENTS CoreFoundation CoreGraphics CoreServices CoreText ImageIO)

include(CoreGraphics.cmake)

# CoreServices provides kUTTypePNG on macOS (guarded by __APPLE__ in PlatformImageCG.cpp).
list(APPEND ImageDiff_LIBRARIES
    Apple::CoreServices
)

# kUTTypePNG deprecated in 12.0; our deployment target is 26.2.
list(APPEND ImageDiff_COMPILE_OPTIONS -Wno-deprecated-declarations)
