list(APPEND PAL_FORWARDING_HEADERS_DIRECTORIES
    spi/cf
)

if (${USE_DIRECT2D})
else ()
    list(APPEND PAL_FORWARDING_HEADERS_DIRECTORIES
        spi/cg
    )
endif ()

list(APPEND PAL_SOURCES
    avfoundation/MediaTimeAVFoundation.cpp

    cf/CoreMediaSoftLink.cpp
)

list(APPEND PAL_PRIVATE_INCLUDE_DIRECTORIES
    "${PAL_DIR}/pal/cf"
)
