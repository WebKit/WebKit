list(APPEND ImageDiff_SOURCES
    skia/PlatformImageSkia.cpp
)

list(APPEND ImageDiff_PRIVATE_LIBRARIES
    SkiaMalloc
)

list(APPEND ImageDiff_PRIVATE_DEFINITIONS USE_SKIA=1)
