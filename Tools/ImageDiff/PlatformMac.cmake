list(APPEND IMAGE_DIFF_LIBRARIES
    CFNetwork
    CoreGraphics
    CoreText
)
set(IMAGE_DIFF_SOURCES
    ${IMAGE_DIFF_DIR}/cg/ImageDiff.cpp
)
list(APPEND ImageDiff_LIBRARIES
    CoreFoundation
    CoreGraphics
    CoreText
)