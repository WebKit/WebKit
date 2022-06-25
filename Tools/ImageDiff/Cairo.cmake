list(APPEND ImageDiff_SOURCES
    cairo/PlatformImageCairo.cpp
)

list(APPEND ImageDiff_LIBRARIES
    Cairo::Cairo
)

list(APPEND ImageDiff_PRIVATE_DEFINITIONS USE_CAIRO=1)
