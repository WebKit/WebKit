list(APPEND ImageDiff_SOURCES
    ${IMAGE_DIFF_DIR}/haiku/PlatformImageHaiku.cpp
)

list(APPEND ImageDiff_INCLUDE_DIRECTORIES
)

list(APPEND ImageDiff_LIBRARIES
    be translation
)

list(APPEND ImageDiff_PRIVATE_DEFINITIONS USE_HAIKU=1)
