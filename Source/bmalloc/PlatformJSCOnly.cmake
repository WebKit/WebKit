if (APPLE)
    list(APPEND bmalloc_SOURCES
        bmalloc/ProcessCheck.mm
    )

    list(APPEND bmalloc_LIBRARIES
        "-framework Foundation"
        objc
    )
endif ()
