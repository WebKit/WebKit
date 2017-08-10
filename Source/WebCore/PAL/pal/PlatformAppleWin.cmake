list(APPEND PAL_FORWARDING_HEADERS_DIRECTORIES
    spi/cf
)

if (${USE_DIRECT2D})
else ()
    list(APPEND PAL_FORWARDING_HEADERS_DIRECTORIES
        spi/cg
    )
endif ()
