list(APPEND ImageDiff_SOURCES
    cg/PlatformImageCG.cpp
)

list(APPEND ImageDiff_LIBRARIES
    Apple::CoreFoundation
    Apple::CoreGraphics
    Apple::CoreText
)
