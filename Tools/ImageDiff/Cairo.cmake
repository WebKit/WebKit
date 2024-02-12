list(APPEND ImageDiff_SOURCES
    cairo/PlatformImageCairo.cpp
)

if (NOT TARGET Cairo::Cairo)
    find_package(Cairo REQUIRED)
endif ()

list(APPEND ImageDiff_LIBRARIES
    Cairo::Cairo
)

list(APPEND ImageDiff_PRIVATE_DEFINITIONS USE_CAIRO=1)
